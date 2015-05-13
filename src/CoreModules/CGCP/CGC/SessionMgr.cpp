/*
    MYCP is a HTTP and C++ Web Application Server.
    Copyright (C) 2009-2010  Akee Yang <akee.yang@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef WIN32
#include <Windows.h>
#include <tchar.h>
#else
#include "../../../CGCClass/tchar.h"
#endif // WIN32

#include "SessionMgr.h"
#include "ResponseImpl.h"
#include <algorithm>
#include <time.h>
//#include <tchar.h>
#ifndef WIN32
#include "dlfcn.h"
#endif
#pragma warning(disable:4311 4312)
#include <ThirdParty/stl/rsa.h>

// CSessionImpl
CSessionImpl::CSessionImpl(const ModuleItem::pointer& pModuleItem,const cgcRemote::pointer& pcgcRemote, const cgcParserBase::pointer& pcgcParser)
: m_bKilled(false)
, m_bIsNewSession(true),m_bRemoteClosed(false)
, m_pProcPrevData(NULL)
//, m_fpFreeParser(NULL)
, m_tNewProcPrevThread(0)
, m_tPrevDataTime(0)
, m_pcgcRemote(pcgcRemote), m_pcgcParser(pcgcParser)
, m_sSessionId(_T(""))
, m_interval(DEFAULT_MAX_INACTIVE_INTERVAL)
//, m_sAccount(_T(""))
//, m_sPasswd(_T(""))
, m_sPrevData("")

{
	BOOST_ASSERT (m_pcgcRemote.get() != NULL);
	//BOOST_ASSERT (m_pcgcParser.get() != NULL);

	m_tCreationTime = time(0);
	m_tLastAccessedtime = m_tCreationTime;
	m_nDataIndex = 0;
	m_tLastSeq = 0;
	m_nCurrentSeq = 0;

	addModuleItem(pModuleItem,pcgcRemote);
	if (pcgcRemote->getProtocol()&PROTOCOL_HTTP)
	{
		SetSessionId(pcgcParser);
	}else
	{
		static unsigned short the_sid_index=0;
		the_sid_index++;
		char lpBuf[18];
		memset(lpBuf, 0, 18);
		sprintf(lpBuf, "%08X%02X%02X",(int)m_tCreationTime,the_sid_index%0xFF,m_pcgcRemote->getRemoteId()%0xFF);
		m_sSessionId = lpBuf;
	}

	memset(&m_pReceiveSeqMasks,-1,sizeof(m_pReceiveSeqMasks));
	//for (int i=0; i<MAX_SEQ_MASKS_SIZE; i++)
	//	m_pReceiveSeqMasks[i] = -1;
}

CSessionImpl::~CSessionImpl(void)
{
	// 用于线程退出标识
	m_bKilled = true;
	m_attributes.reset();
	m_pcgcRemote.reset();
	m_pcgcParser.reset();
	m_pSessionModuleList.clear();
	m_pHoldResponseList.clear();
	delPrevDatathread(true);
}

bool CSessionImpl::OnRunCGC_Session_Open(const ModuleItem::pointer& pModuleItem,const cgcRemote::pointer& pcgcRemote)
{
	// call CGC_Session_Open
	CSessionModuleInfo::pointer pSessionModuleInfo = addModuleItem(pModuleItem,pcgcRemote);
	if (pSessionModuleInfo.get() == NULL) return false;
	if (pSessionModuleInfo->m_bOpenSessioned) return true;
	void * hModule = pModuleItem->getModuleHandle();
#ifdef WIN32
	FPCGC_Session_Open fp = (FPCGC_Session_Open)GetProcAddress((HMODULE)hModule, "CGC_Session_Open");
#else
	FPCGC_Session_Open fp = (FPCGC_Session_Open)dlsym(hModule, "CGC_Session_Open");
#endif
	if (fp)
	{
		bool ret = false;
		try
		{
			ret = fp(shared_from_this());
		}catch (std::exception const &)
		{
		}catch (...)
		{}
		if (ret)
		{
			pSessionModuleInfo->m_bOpenSessioned = true;
		}else
		{
			m_pSessionModuleList.remove(pModuleItem->getName());
		}
		// exception
		return ret;
	}
	// default true
	return true;
}

void CSessionImpl::OnRunCGC_Remote_Close(unsigned long nRemoteId, int nErrorCode)
{
	//printf("**** OnRunCGC_Remote_Close remoteid=%d, ,size=%d\n",nRemoteId,m_pSessionModuleList.size());
	bool bIsHttpIe6 = false;
	if (getProtocol()&PROTOCOL_HTTP)
	{
		//printf("**** UserAgent: %s\n",m_sUserAgent.c_str());
		// MSIE 6.
		// MSIE 9.0
		bIsHttpIe6 = m_sUserAgent.find("MSIE ") >= 0;
		//bIsHttpIe6 = m_sUserAgent.find("MSIE 6.") >= 0;
	}

	BoostReadLock rdlock(m_pSessionModuleList.mutex());
	CLockMap<tstring,CSessionModuleInfo::pointer>::const_iterator pIter=m_pSessionModuleList.begin();
	for (;pIter!=m_pSessionModuleList.end(); pIter++)
	{
		CSessionModuleInfo::pointer pSessionModuleInfo = pIter->second;
		OnRunCGC_Remote_Close(pSessionModuleInfo,nRemoteId,nErrorCode);
		if (bIsHttpIe6 && (nErrorCode == 104 ||	// Connection reset by peer
			nErrorCode == 2)) 		// End of file
			continue;
		if (pSessionModuleInfo->m_pRemote->getRemoteId() == nRemoteId)
			pSessionModuleInfo->m_pRemote->invalidate();
	}
}
void CSessionImpl::OnRunCGC_Remote_Change(const cgcRemote::pointer& pcgcRemote)
{
	{
		BoostReadLock rdlock(m_pSessionModuleList.mutex());
		CLockMap<tstring,CSessionModuleInfo::pointer>::const_iterator pIter=m_pSessionModuleList.begin();
		for (;pIter!=m_pSessionModuleList.end(); pIter++)
		{
			CSessionModuleInfo::pointer pSessionModuleInfo = pIter->second;
			OnRunCGC_Remote_Change(pSessionModuleInfo,pcgcRemote);
		}
	}
}

void CSessionImpl::OnRunCGC_Remote_Change(const CSessionModuleInfo::pointer& pSessionModuleInfo, const cgcRemote::pointer& pcgcRemote)
{
	if (pSessionModuleInfo->m_bOpenSessioned)
	{
		pSessionModuleInfo->m_bOpenSessioned = false;
		//printf("**** CGC_Session_Close %s\n",getId().c_str());
		ModuleItem::pointer pModuleItem = pSessionModuleInfo->m_pModuleItem;
		void * hModule = pModuleItem->getModuleHandle();
#ifdef WIN32
		FPCGC_Remote_Change fp = (FPCGC_Remote_Change)GetProcAddress((HMODULE)hModule, "CGC_Remote_Change");
#else
		FPCGC_Remote_Change fp = (FPCGC_Remote_Change)dlsym(hModule, "CGC_Remote_Change");
#endif
		if (fp)
		{
			try
			{
				fp(shared_from_this(),pcgcRemote);
			}catch (std::exception const &)
			{
			}catch (...)
			{}
		}
	}
}

void CSessionImpl::OnRunCGC_Remote_Close(const CSessionModuleInfo::pointer& pSessionModuleInfo,unsigned long nRemoteId,int nErrorCode)
{
	if (pSessionModuleInfo->m_pRemoteList.remove(nRemoteId))
	{
		// **该错误不能处理，否则IE6会有问题；
		if (getProtocol()&PROTOCOL_HTTP)
		{
			//printf("**** UserAgent: %s\n",m_sUserAgent.c_str());
			if ((nErrorCode == 104 ||		// Connection reset by peer
				nErrorCode == 2) && 		// End of file
				m_sUserAgent.find("MSIE ") >= 0)
				return;

			// MSIE 6.
			// MSIE 9.0
			//const bool bIsHttpIe6 = m_sUserAgent.find("MSIE ") >= 0;
			//if (bIsHttpIe6 && (nErrorCode == 104 ||	// Connection reset by peer
			//	nErrorCode == 2)) 		// End of file
			//	return;
		}
		//printf("**** OnRunCGC_Remote_Close remove modulete_remoteid=%d\n",nRemoteId);
		ModuleItem::pointer pModuleItem = pSessionModuleInfo->m_pModuleItem;
		void * hModule = pModuleItem->getModuleHandle();
		if (hModule == NULL) return;
#ifdef WIN32
		FPCGC_Remote_Close fp = (FPCGC_Remote_Close)GetProcAddress((HMODULE)hModule, "CGC_Remote_Close");
#else
		FPCGC_Remote_Close fp = (FPCGC_Remote_Close)dlsym(hModule, "CGC_Remote_Close");
#endif
		if (fp)
		{
			try
			{
				fp(shared_from_this(),nRemoteId);
			}catch (std::exception const &)
			{
			}catch (...)
			{}
		}
	}
}

void CSessionImpl::OnRunCGC_Session_Close(void)
{
	// call CGC_Session_Close
	{
		BoostReadLock rdlock(m_pSessionModuleList.mutex());
		CLockMap<tstring,CSessionModuleInfo::pointer>::const_iterator pIter=m_pSessionModuleList.begin();
		for (;pIter!=m_pSessionModuleList.end(); pIter++)
		{
			CSessionModuleInfo::pointer pSessionModuleInfo = pIter->second;
			OnRunCGC_Session_Close(pSessionModuleInfo);
		}
	}
	m_pSessionModuleList.clear(false);
	m_pHoldResponseList.clear();
}
void CSessionImpl::OnRunCGC_Session_Close(const CSessionModuleInfo::pointer& pSessionModuleInfo)
{
	if (pSessionModuleInfo->m_bOpenSessioned)
	{
		pSessionModuleInfo->m_bOpenSessioned = false;
		//printf("**** CGC_Session_Close %s\n",getId().c_str());
		ModuleItem::pointer pModuleItem = pSessionModuleInfo->m_pModuleItem;
		void * hModule = pModuleItem->getModuleHandle();
#ifdef WIN32
		FPCGC_Session_Close fp = (FPCGC_Session_Close)GetProcAddress((HMODULE)hModule, "CGC_Session_Close");
#else
		FPCGC_Session_Close fp = (FPCGC_Session_Close)dlsym(hModule, "CGC_Session_Close");
#endif
		if (fp)
		{
			try
			{
				fp(shared_from_this());
			}catch (std::exception const &)
			{
			}catch (...)
			{}
		}
	}
}
void CSessionImpl::OnServerStop(void)
{
	//OnRunCGC_Session_Close();
	m_pcgcRemote->invalidate();
}

void CSessionImpl::ProcHoldResponseTimeout(void)
{
	if (!m_pHoldResponseList.empty())
	{
		BoostWriteLock wtlock(m_pHoldResponseList.mutex());
		CLockMap<unsigned long,CHoldResponseInfo::pointer>::iterator pIter = m_pHoldResponseList.begin();
		for (; pIter!=m_pHoldResponseList.end(); pIter++)
		{
			CHoldResponseInfo::pointer pHoldResponse = pIter->second;
			if (!pHoldResponse->IsExpireHold())
			{
				continue;
			}
			// 已经过期
			//printf("**** ProcHoldResponseTimeout %d\n",(int)pHoldResponse->m_tExpireTime);
			m_pHoldResponseList.erase(pIter);
			if (m_pHoldResponseList.empty())
				break;
			else
				pIter = m_pHoldResponseList.begin();
		}
	}
}

void CSessionImpl::HoldResponse(const cgcResponse::pointer& pResponse, int nHoldSecond)
{
	const unsigned long nRemoteId = pResponse->getRemoteId();
	CHoldResponseInfo::pointer pHoldResponse;
	const bool bRemote = (bool)(nHoldSecond<=0);
	if (m_pHoldResponseList.find(nRemoteId,pHoldResponse,bRemote))
	{
		if (!bRemote)
		{
			pHoldResponse->SetHoldSecond(nHoldSecond);
		}
	}else if (nHoldSecond > 0)
	{
		m_pHoldResponseList.insert(nRemoteId,CHoldResponseInfo::create(pResponse,nHoldSecond));
	}
}
void CSessionImpl::removeResponse(unsigned long nRemoteId)
{
	m_pHoldResponseList.remove(nRemoteId);
}

void CSessionImpl::onRemoteClose(unsigned long remoteId,int nErrorCode)
{
	// 只清除connection，不清除session
	OnRunCGC_Remote_Close(remoteId,nErrorCode);
	m_bRemoteClosed = true;
	bool bNotRemoteConnected = true;	// 会话是否已经没有REMOTE连接
	//{
	//	boost::mutex::scoped_lock lock(m_pSessionModuleList.mutex());
	//	CLockMap<tstring,CSessionModuleInfo::pointer>::iterator pIter=m_pSessionModuleList.begin();
	//	for (;pIter!=m_pSessionModuleList.end(); pIter++)
	//	{
	//		CSessionModuleInfo::pointer pSessionModuleInfo = pIter->second;
	//		if (pSessionModuleInfo->m_pRemote->getRemoteId() == remoteId)
	//		{
	//			pSessionModuleInfo->m_pRemote->invalidate();	// 清除connection
	//			//OnRunCGC_Session_Close(pSessionModuleInfo);
	//			//m_pSessionModuleList.erase(pIter);
	//			//if (m_pSessionModuleList.empty())
	//			//	break;
	//			//else
	//			//	pIter = m_pSessionModuleList.begin();
	//		}else
	//		{
	//			bNotRemoteConnected = false;
	//		}
	//	}
	//}
	//printf("**** onRemoteClose 1111\n");
	m_pHoldResponseList.remove(remoteId);
	if (m_pcgcRemote.get() != NULL && m_pcgcRemote->getRemoteId() == remoteId)
	{
		//if (!m_pcgcRemote->isInvalidate() && (getProtocol()&PROTOCOL_HTTP))
		//{
		//	cgcHttpResponse::pointer pResponse = cgcHttpResponse::pointer(new CHttpResponseImpl(m_pcgcRemote, CGC_PARSERHTTPSERVICE_DEF(m_pcgcParser)));
		//	pResponse->sendResponse(STATUS_CODE_200);
		//}
		if ((nErrorCode == 104 ||		// Connection reset by peer
			nErrorCode == 2) && 		// End of file
			(getProtocol()&PROTOCOL_HTTP)==PROTOCOL_HTTP &&	m_sUserAgent.find("MSIE ") >= 0)
		{
			// **
		}else
		{
			m_pcgcRemote->invalidate();	// 清除connection
		}

		//bool bIsHttpIe6 = false;
		//if (getProtocol()&PROTOCOL_HTTP)
		//{
		//	//printf("**** UserAgent: %s\n",m_sUserAgent.c_str());
		//	// MSIE 6.
		//	// MSIE 9.0
		//	bIsHttpIe6 = m_sUserAgent.find("MSIE ") >= 0;
		//	//bIsHttpIe6 = m_sUserAgent.find("MSIE 6.") >= 0;
		//}
		//if (bIsHttpIe6 && (nErrorCode == 104 ||	// Connection reset by peer
		//	nErrorCode == 2)) 		// End of file
		//{
		//	// ***
		//}else
		//{
		//	m_pcgcRemote->invalidate();	// 清除connection
		//}
	}else if (m_pcgcRemote.get() != NULL && !m_pcgcRemote->isInvalidate())
	{
		bNotRemoteConnected = false;
	}
	//printf("**** SessionMgr.onRemoteClose end\n");
	//if (bNotRemoteConnected)
	//{
	//	// 已经没有REMOTE连接，设置UserAgent为空
	//	m_sUserAgent.clear();
	//}
	//if (m_pSessionModuleList.empty())
	//{
	//	invalidate();
	//}else if (getRemoteId() == remoteId)
	//{
	//	// ?
	//	//boost::mutex::scoped_lock lock(m_pSessionModuleList.mutex());
	//	//CLockMap<tstring,CSessionModuleInfo::pointer>::iterator pIter=m_pSessionModuleList.begin();
	//	//for (;pIter!=m_pSessionModuleList.end(); pIter++)
	//	//invalidate();
	//}
}

const tstring& CSessionImpl::SetSessionId(const cgcParserBase::pointer& pcgcParser)
{
	if (getProtocol()&PROTOCOL_HTTP)
	{
		cgcParserHttp::pointer pHttpParser = CGC_PARSERHTTPSERVICE_DEF(pcgcParser);
		m_sSessionId = pHttpParser->getCookieMySessionId();
		// User-Agent: Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0; InfoPath.2; CIBA; .NET CLR 2.0.50727; .NET CLR 3.0.04506.648; .NET CLR 3.5.21022)
		m_sUserAgent = pHttpParser->getHeader(Http_UserAgent,"");
	}
	return this->getId();
}

void CSessionImpl::SetSslPublicKey(const tstring& newValue)
{
	m_sSslPublicKey = newValue;
	if (m_sSslPublicKey.empty())
	{
		m_sSslPassword.clear();
	}else if (m_sSslPassword.empty())
	{
		m_sSslPassword = GetSaltString(24);	// 24*8=128Bit 足够了
	}
}

void CSessionImpl::invalidate(void)
{
	OnRunCGC_Session_Close();
	m_pcgcRemote->invalidate();
}

void CSessionImpl::setMaxInactiveInterval(int interval)
{
	m_interval = interval;
}

cgcAttributes::pointer CSessionImpl::getAttributes(bool create)
{
	if (create && m_attributes.get() == NULL)
		m_attributes = cgcAttributes::pointer(new AttributesImpl());
	return m_attributes;
}

//cgcSession::pointer CSessionImpl::cgc_shared_from_this(void)
//{
//	return shared_from_this();
//}

cgcResponse::pointer CSessionImpl::getLastResponse(const tstring& moduleName) const
{
	CSessionModuleInfo::pointer pSessionModuleItem;
	if (!moduleName.empty() && m_pSessionModuleList.find(moduleName,pSessionModuleItem))
	{
		if (m_pcgcRemote->getProtocol() == PROTOCOL_SOTP)
		{
			CSotpResponseImpl* pSotpResponseImpl = new CSotpResponseImpl(pSessionModuleItem->m_pRemote, CGC_PARSERSOTPSERVICE_DEF(m_pcgcParser), (CResponseHandler*)this);
			pSotpResponseImpl->setSession(const_cast<CSessionImpl*>(this)->shared_from_this());
			return cgcSotpResponse::pointer(pSotpResponseImpl);
		}else if (m_pcgcRemote->getProtocol() & PROTOCOL_HTTP)
			return cgcResponse::pointer(new CHttpResponseImpl(pSessionModuleItem->m_pRemote, CGC_PARSERHTTPSERVICE_DEF(m_pcgcParser)));
		else
			return cgcResponse::pointer(new CResponseImpl(pSessionModuleItem->m_pRemote, CGC_PARSERHTTPSERVICE_DEF(m_pcgcParser)));
	}else
	{
		if (m_pcgcRemote->getProtocol() == PROTOCOL_SOTP)
		{
			CSotpResponseImpl* pSotpResponseImpl = new CSotpResponseImpl(m_pcgcRemote, CGC_PARSERSOTPSERVICE_DEF(m_pcgcParser), (CResponseHandler*)this);
			pSotpResponseImpl->setSession(const_cast<CSessionImpl*>(this)->shared_from_this());
			return cgcSotpResponse::pointer(pSotpResponseImpl);
		}else if (m_pcgcRemote->getProtocol() & PROTOCOL_HTTP)
			return cgcNullResponse;//cgcResponse::pointer(new CHttpResponseImpl(m_pcgcRemote, CGC_PARSERHTTPSERVICE_DEF(m_pcgcParser)));
		else
			return cgcResponse::pointer(new CResponseImpl(m_pcgcRemote, CGC_PARSERHTTPSERVICE_DEF(m_pcgcParser)));
	}
}

cgcResponse::pointer CSessionImpl::getHoldResponse(unsigned long nRemoteId)
{
	CHoldResponseInfo::pointer result;
	if (m_pHoldResponseList.find(nRemoteId,result,true))
	{
		return result->m_pHoldResponse;
	}
	return cgcNullResponse;
}

void CSessionImpl::setDataResponseImpl(const tstring& sModuleName,const cgcRemote::pointer& pcgcRemote, const cgcParserBase::pointer& pcgcParser)
{
	if (pcgcRemote.get() == NULL || pcgcRemote->isInvalidate() || pcgcParser.get() == NULL) return;
	m_pcgcParser = pcgcParser;
	setDataResponseImpl(sModuleName,pcgcRemote);
}
void CSessionImpl::setDataResponseImpl(const tstring& sModuleName,const cgcRemote::pointer& pcgcRemote)
{
	if (pcgcRemote.get() == NULL || pcgcRemote->isInvalidate()) return;
	m_bIsNewSession = false;
	m_tLastAccessedtime = time(0);
	//if (pcgcRemote->getProtocol() & PROTOCOL_HTTP)
	if (m_pcgcRemote->getRemoteId() != pcgcRemote->getRemoteId())
	{
		//printf("**** setDataResponseImpl remoteid=%d\n",pcgcRemote->getRemoteId());
		CSessionModuleInfo::pointer pSessionModuleItem;
		if (sModuleName.empty())
		{
			AUTO_RLOCK(m_pSessionModuleList);
			CLockMap<tstring,CSessionModuleInfo::pointer>::const_iterator pIter = m_pSessionModuleList.begin();
			if (pIter != m_pSessionModuleList.end())
			{
				pSessionModuleItem = pIter->second;
				pSessionModuleItem->m_pRemote = pcgcRemote;
				pSessionModuleItem->m_pRemoteList.insert(pcgcRemote->getRemoteId(), true);
			}
		}else if (m_pSessionModuleList.find(sModuleName,pSessionModuleItem))
		{
			pSessionModuleItem->m_pRemote = pcgcRemote;
			pSessionModuleItem->m_pRemoteList.insert(pcgcRemote->getRemoteId(), true);
		}
		m_pcgcRemote = pcgcRemote;
	}
}

void do_prevdata(CSessionImpl * pSessionImpl)
{
	assert (pSessionImpl != 0);

	// 该线程很快结束，最多不超过二秒钟，所以不用主动管理它。
	try
	{
		time_t tNewPrevDataThreadTime = pSessionImpl->getNewPrevDataThreadTime();
		while (!pSessionImpl->IsKilled())
		{
			time_t tNewPrevDataThreadTimeNow = pSessionImpl->getNewPrevDataThreadTime();
			// 已经不是当前线程要处理的数据
			if (tNewPrevDataThreadTime != tNewPrevDataThreadTimeNow) return;

			time_t tNowPrevDataTime = pSessionImpl->getPrevDataTime();
			// 已经没有当前线程要处理的数据
			if (tNowPrevDataTime == 0) return;

			// 二秒钟内，继续等待......................
			if (time(0) <= tNowPrevDataTime + 2)
			{
#ifdef WIN32
				Sleep(1000);
#else
				sleep(1);
#endif
				continue;
			}

			cgcResponse::pointer responseImpl = ((cgcSession*)pSessionImpl)->getLastResponse("");
			// 已经处理完成返回
			if (responseImpl.get() == NULL) return;
			// 已经超过二秒钟，并且还没有返回，返回数据并退出线程
			if (((cgcSession*)pSessionImpl)->getProtocol() == PROTOCOL_SOTP)
			{
				((CSotpResponseImpl*)responseImpl.get())->sendAppCallResult(-106);
			}
			pSessionImpl->clearPrevData();
			break;
		}
	}catch (const std::exception &)
	{
	}catch (...)
	{}
}

void CSessionImpl::newPrevDataThread(void)
{
	if (NULL == m_pProcPrevData)
	{
		m_tNewProcPrevThread = time(0);
		boost::thread_attributes attrs;
		attrs.set_stack_size(CGC_THREAD_STACK_MIN);
		m_pProcPrevData = new boost::thread(attrs,boost::bind(&do_prevdata, this));
	}
}

void CSessionImpl::delPrevDatathread(bool bJoin)
{
	boost::thread *	pProcPrevDataTemp = m_pProcPrevData;
	m_pProcPrevData = NULL;
	if (pProcPrevDataTemp)
	{
		if (bJoin)
			pProcPrevDataTemp->join();
		delete pProcPrevDataTemp;
	}
}

CSessionModuleInfo::pointer CSessionImpl::addModuleItem(const ModuleItem::pointer& pModuleItem,const cgcRemote::pointer& wssRemote)
{
	CSessionModuleInfo::pointer pSessionModuleItem;
	if (pModuleItem.get()==NULL || wssRemote.get()==NULL) return pSessionModuleItem;
	if (!m_pSessionModuleList.find(pModuleItem->getName(),pSessionModuleItem))
	{
		pSessionModuleItem = CSessionModuleInfo::create(pModuleItem,wssRemote);
		m_pSessionModuleList.insert(pModuleItem->getName(),pSessionModuleItem);
	}else
	{
		pSessionModuleItem->m_pRemote = wssRemote;
	}
	pSessionModuleItem->m_pRemoteList.insert(wssRemote->getRemoteId(),true);
	return pSessionModuleItem;
}
ModuleItem::pointer CSessionImpl::getModuleItem(const tstring& sModuleName, bool bGetDefault) const
{
	ModuleItem::pointer result;
	CSessionModuleInfo::pointer pSessionModuleItem;
	if (!sModuleName.empty() && m_pSessionModuleList.find(sModuleName,pSessionModuleItem))
	{
		return pSessionModuleItem->m_pModuleItem;
	}else if(bGetDefault && !m_pSessionModuleList.empty())
	{
		AUTO_CONST_RLOCK(m_pSessionModuleList);
		CLockMap<tstring,CSessionModuleInfo::pointer>::const_iterator pIter = m_pSessionModuleList.begin();
		if (pIter != m_pSessionModuleList.end())
		{
			result = pIter->second->m_pModuleItem;
		}
	}
	return result;
}

bool CSessionImpl::ProcessDataResend(void)
{
	BoostReadLock rdlock(m_mapSeqInfo.mutex());
	CLockMap<unsigned short, cgcSeqInfo::pointer>::iterator pIter;
	for (pIter=m_mapSeqInfo.begin(); pIter!=m_mapSeqInfo.end(); pIter++)
	{
		cgcSeqInfo::pointer pCidInfo = pIter->second;
		if (pCidInfo->isTimeout())
		{
			if (pCidInfo->canResendAgain() && !m_pcgcRemote->isInvalidate())
			{
				pCidInfo->increaseResends();
				pCidInfo->setSendTime();
				// resend
				m_pcgcRemote->sendData((const unsigned char*)pCidInfo->getCallData(), pCidInfo->getDataSize());

				//if (m_pHandler)
				//	m_pHandler->OnCidTimeout(pCidInfo->getCid(), pCidInfo->getSign(), true);
			}else
			{
				// 
				unsigned short nSeq = pIter->first;
				rdlock.unlock();
				m_mapSeqInfo.remove(nSeq);

				//// OnCidResend
				//if (m_pHandler)
				//	m_pHandler->OnCidTimeout(pCidInfo->getCid(), pCidInfo->getSign(), false);
			}
			return true;
		}
	}
	return false;
}
void CSessionImpl::ProcessAckSeq(unsigned short seq)
{
	m_mapSeqInfo.remove(seq);
}
bool CSessionImpl::ProcessDuplationSeq(unsigned short seq)
{
	boost::mutex::scoped_lock lock(m_recvSeq);
	if (m_tLastSeq>0 && (m_tLastAccessedtime-m_tLastSeq)>12)
	{
		memset(&m_pReceiveSeqMasks,-1,sizeof(m_pReceiveSeqMasks));
		//for (int i=0; i<MAX_SEQ_MASKS_SIZE; i++)
		//	m_pReceiveSeqMasks[i] = -1;
	}else
	{
		for (int i=0; i<MAX_SEQ_MASKS_SIZE; i++)
		{
			// Receive duplation message, 
			if (m_pReceiveSeqMasks[i] == (int)seq)
			{
				m_tLastSeq = m_tLastAccessedtime;
				return true;
			}
		}
	}
	m_tLastSeq = m_tLastAccessedtime;
	m_nDataIndex++;
	m_pReceiveSeqMasks[m_nDataIndex%MAX_SEQ_MASKS_SIZE] = seq;
	//if (bNeedAck)
	//{
	//	cgcParserSotp::pointer pcgcParser = CGC_PARSERSOTPSERVICE_DEF(m_pcgcParser);
	//	tstring responseData = pcgcParser->getAckResult(seq);
	//	m_pcgcRemote->sendData((const unsigned char*)responseData.c_str(), responseData.length());
	//}
	return false;
}

const std::string & CSessionImpl::addPrevData(const char * sUnPrecData)
{
	if (sUnPrecData != NULL)
	{
		m_sPrevData.append(sUnPrecData);
		m_tPrevDataTime = time(0);
	}

	return m_sPrevData;
}

int CSessionImpl::onAddSeqInfo(const unsigned char * callData, unsigned int dataSize, unsigned short seq, unsigned long cid, unsigned long sign)
{
	//if (m_timeoutResends <= 0 || m_timeoutSeconds <= 0) return;
	if (callData == 0 || dataSize == 0) return -1;

	cgcSeqInfo::pointer pSeqInfo;
	if (m_mapSeqInfo.find(seq, pSeqInfo))
	{
		pSeqInfo->setCallData(callData, dataSize);
		//pSeqInfo->setTimeoutResends(m_timeoutResends);
		//pSeqInfo->setTimeoutSeconds(m_timeoutSeconds);
		pSeqInfo->setSign(sign);
		pSeqInfo->setSendTime();
	}else
	{
		pSeqInfo = cgcSeqInfo::create(seq, cid, sign, 5, 4);
		pSeqInfo->setCallData(callData, dataSize);
		m_mapSeqInfo.insert(seq, pSeqInfo);
	}
	pSeqInfo->setSessionId(this->m_sSessionId);
	return 0;
}

unsigned short CSessionImpl::onGetNextSeq(void)
{
	boost::mutex::scoped_lock lock(m_mutexReq);
	return ++m_nCurrentSeq;
}

int CSessionImpl::onAddSeqInfo(unsigned char * callData, unsigned int dataSize, unsigned short seq, unsigned long cid, unsigned long sign)
{
	//if (m_timeoutResends <= 0 || m_timeoutSeconds <= 0) return false;
	if (callData == 0 || dataSize == 0) return -1;

	cgcSeqInfo::pointer pSeqInfo;
	if (m_mapSeqInfo.find(seq, pSeqInfo))
	{
		pSeqInfo->setCallData(callData, dataSize);
		//pSeqInfo->setTimeoutResends(m_timeoutResends);
		//pSeqInfo->setTimeoutSeconds(m_timeoutSeconds);
		pSeqInfo->setSign(sign);
		pSeqInfo->setSendTime();
	}else
	{
		pSeqInfo = cgcSeqInfo::create(seq, cid, sign, 5, 4);
		pSeqInfo->setCallData(callData, dataSize);
		m_mapSeqInfo.insert(seq, pSeqInfo);
	}
	pSeqInfo->setSessionId(this->m_sSessionId);
	return 0;
}

//
//void CSessionImpl::setAccontInfo(const tstring & sAccount, const tstring & sPasswd)
//{
//	this->m_sAccount = sAccount;
//	this->m_sPasswd = sPasswd;
//}

// CSessionMgr
CSessionMgr::CSessionMgr()
{
}

CSessionMgr::~CSessionMgr(void)
{
	invalidates(false);
	FreeHandle();
}

cgcSession::pointer CSessionMgr::SetSessionImpl(const ModuleItem::pointer& pModuleItem,const cgcRemote::pointer& pcgcRemote,const cgcParserBase::pointer& pcgcParser)
{
	cgcSession::pointer result;
	if (pcgcRemote->isInvalidate()) return result;
	unsigned long nRemoteId = pcgcRemote->getRemoteId();
 
	result = GetSessionImplByRemote(nRemoteId);
	if (result.get() == NULL)
	{
		result = SESSIONIMPL(pModuleItem,pcgcRemote, pcgcParser);
		// 记录二个MAP 表
		m_mapSessionImpl.insert(result->getId(), result);
		m_mapRemoteSessionId.insert(nRemoteId, result->getId());
	}else
	{
		((CSessionImpl*)result.get())->setDataResponseImpl(pModuleItem->getName(),pcgcRemote, pcgcParser);
		// 记录二个MAP 表
		//m_mapSessionImpl.insert(result->getId(), result);
		//m_mapRemoteSessionId.insert(nRemoteId, result->getId());
	}

	return result;
}

cgcSession::pointer CSessionMgr::GetSessionImplByRemote(unsigned long remoteId)
{
	cgcSession::pointer result;
	if (remoteId == 0) return result;

#ifdef USES_MULTI_REMOTE_SESSION
	std::vector<tstring> sSessionList;
	if (!m_mapRemoteSessionId.find(remoteId, sSessionList, false))
	{
		return result;
	}
	for (size_t i=0;i<sSessionList.size();i++)
	{
		tstring sessionid = sSessionList[i];
		result = GetSessionImpl(sessionid);
		if (result.get() != NULL)
			break;
	}
#else
	// 先remoteid map查sessionid
	tstring sessionid = _T("");
	if (!m_mapRemoteSessionId.find(remoteId, sessionid, false))
		return result;

	// pRemoteIdIter->second: sessionid
	result = GetSessionImpl(sessionid);
	if (result.get() == NULL)
	{
		// 该remoteid 项已经无效
		m_mapRemoteSessionId.remove(remoteId);
	}
#endif
	return result;
}

cgcSession::pointer CSessionMgr::GetSessionImpl(const tstring & sSessionId) const
{
	cgcSession::pointer result;
	m_mapSessionImpl.find(sSessionId, result);
	return result;
}
//void CSessionMgr::ChangeSessionId(const tstring& sOldSessionId,const cgcRemote::pointer& wssRemote, const cgcParserBase::pointer& pcgcParser)
//{
//	cgcSession::pointer pSession;
//	if (m_mapSessionImpl.find(sOldSessionId, pSession,true))
//	{
//		unsigned long nOldRemoteId = pSession->getRemoteId();
//		m_mapRemoteSessionId.remove(nOldRemoteId);
//	
//		CSessionImpl * pSessionImpl = (CSessionImpl*)pSession.get();
//		const tstring sNewSessionId = pSessionImpl->SetSessionId(pcgcParser);
//		m_mapSessionImpl.insert(sNewSessionId,pSession);
//		m_mapRemoteSessionId.insert(wssRemote->getRemoteId(),sNewSessionId);
//	}
//}
void CSessionMgr::SetRemoteSession(unsigned long remoteId,const tstring& sSessionId)
{
//#ifndef USES_MULTI_REMOTE_SESSION
//	m_mapRemoteSessionId.remove(remoteId);
//#endif
	m_mapRemoteSessionId.insert(remoteId, sSessionId);
}

void CSessionMgr::RemoveSessionImpl(const cgcSession::pointer & sessionImpl)
{
	assert (sessionImpl.get() != NULL);
	// delete remoteid map
	m_mapRemoteSessionId.remove(sessionImpl->getRemoteId());
	// delete sessionid map
	m_mapSessionImpl.remove(sessionImpl->getId());
}

//void CSessionMgr::setInterval(unsigned long remoteId, int interval)
//{
//	cgcSession::pointer pSessionImpl = GetSessionImplByRemote(remoteId);
//	if (pSessionImpl.get() != NULL)
//	{
//		if (pSessionImpl->getProtocol() == PROTOCOL_SOTP)
//			interval = 30;
//		else if ((pSessionImpl->getProtocol() & PROTOCOL_HTTP) != PROTOCOL_HTTP)
//			interval = 5;
//		pSessionImpl->setMaxInactiveInterval(interval);
//	}
//}

void CSessionMgr::onRemoteClose(unsigned long remoteId, int nErrorCode)
{
	// 只清除connection，不清除session
#ifdef USES_MULTI_REMOTE_SESSION
	std::vector<tstring> sSessionList;
	if (!m_mapRemoteSessionId.find(remoteId, sSessionList, true))
	{
		return;
	}
	for (size_t i=0;i<sSessionList.size();i++)
	{
		const tstring sessionid = sSessionList[i];
		cgcSession::pointer pSessionImpl = GetSessionImpl(sessionid);
		if (pSessionImpl.get() != NULL)
		{
			((CSessionImpl*)pSessionImpl.get())->onRemoteClose(remoteId,nErrorCode);
		}
	}
#else
	cgcSession::pointer pSessionImpl = GetSessionImplByRemote(remoteId);
	if (pSessionImpl.get() != NULL)
	{
		//printf("**** onRemoteClose: Session '%s'\n", pSessionImpl->getId().c_str());
		((CSessionImpl*)pSessionImpl.get())->onRemoteClose(remoteId,nErrorCode);
		//if (pSessionImpl->isInvalidate())
		//{
		//	this->RemoveSessionImpl(pSessionImpl);
		//}
	}
#endif
}


bool CSessionMgr::ProcDataResend(void)
{
	// lock
	BoostReadLock rdlock(m_mapSessionImpl.mutex());
	cgcSession::pointer pSessionImplTimeout;
	CLockMap<tstring, cgcSession::pointer>::iterator pIter;
	for (pIter=m_mapSessionImpl.begin(); pIter!=m_mapSessionImpl.end(); pIter++)
	{
		cgcSession::pointer sessionImpl = pIter->second;
		CSessionImpl * pSessionImpl = (CSessionImpl*)sessionImpl.get();
		while (pSessionImpl->ProcessDataResend())
		{
			continue;
		}
	}
	return false;
}

void CSessionMgr::ProcLastAccessedTime(std::string& pOutCloseSid)
{
	// lock
	time_t now = time(0);
	cgcSession::pointer pSessionImplTimeout;
	{
		BoostReadLock rdlock(m_mapSessionImpl.mutex());
		CLockMap<tstring, cgcSession::pointer>::iterator pIter;
		for (pIter=m_mapSessionImpl.begin(); pIter!=m_mapSessionImpl.end(); pIter++)
		{
			cgcSession::pointer pSessionImpl = pIter->second;
			((CSessionImpl*)pSessionImpl.get())->ProcHoldResponseTimeout();	// 处理超时hold response
			// local time error
			if (now < pSessionImpl->getLastAccessedtime())
				continue;
			const time_t timeout = now - pSessionImpl->getLastAccessedtime();

			// SESSION 已经失效
			if (timeout > pSessionImpl->getMaxInactiveInterval()*60)
			{
				pSessionImplTimeout =  pSessionImpl;
				break;
			}
		}
	}
	if (pSessionImplTimeout.get() != NULL)
	{
		OnSessionClosedTimeout(pSessionImplTimeout);
		pOutCloseSid = pSessionImplTimeout->getId();
	}
}

void CSessionMgr::invalidates(bool bStop)
{
	CLockMap<tstring, cgcSession::pointer>::iterator pIter;
	for (pIter=m_mapSessionImpl.begin(); pIter!=m_mapSessionImpl.end(); pIter++)
	{
		cgcSession::pointer pSessionImpl = pIter->second;
		if (bStop)
		{
			cgcResponse::pointer responseImpl = pSessionImpl->getLastResponse("");
			if (responseImpl.get()!=NULL)
			{
				if (pSessionImpl->getProtocol() == PROTOCOL_SOTP)
				{
					//((CSotpResponseImpl*)responseImpl.get())->sendSessionResult(-117,pSessionImpl->getId());
					((CSotpResponseImpl*)responseImpl.get())->sendAppCallResult(-117,-1,false);
					((CSessionImpl*)pSessionImpl.get())->OnServerStop();
					continue;
					//printf("***** -117******************\n");
				}else
				{
				}
			}
		}

		pSessionImpl->invalidate();
	}
}

void CSessionMgr::FreeHandle(void)
{
	m_mapSessionImpl.clear();
	m_mapRemoteSessionId.clear();
}

int CSessionMgr::OnSessionClosedTimeout(const cgcSession::pointer & sessionImpl)
{
	assert (sessionImpl.get() != NULL);
	try
	{
		sessionImpl->invalidate();
		RemoveSessionImpl(sessionImpl);
	}catch (const std::exception &)
	{
	}catch (...)
	{}

	return 0;
}
