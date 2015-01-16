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

#include "SotpResponseImpl.h"
//#include "../../CGCBase/cgcString.h"
#include "SessionMgr.h"

CSotpResponseImpl::CSotpResponseImpl(cgcRemote::pointer pcgcRemote, cgcParserSotp::pointer pcgcParser, CResponseHandler * pHandler)
: m_cgcRemote(pcgcRemote)
, m_cgcParser(pcgcParser)
, m_pResponseHandler(pHandler)
, m_bResponseSended(false)
, m_bNotResponse(false)
, m_nHoldSecond(-1)
, m_pSendLock(NULL)
, m_originalCallId(m_cgcParser->getCallid())
, m_originalCallSign(m_cgcParser->getSign())
{
	BOOST_ASSERT (m_cgcRemote.get() != 0);
	BOOST_ASSERT (m_cgcParser.get() != 0);
}

CSotpResponseImpl::~CSotpResponseImpl(void)
{
	m_cgcRemote.reset();
	m_cgcParser.reset();
	if (m_pSendLock)
		delete m_pSendLock;
}

int CSotpResponseImpl::sendSessionResult(long retCode, const tstring & sSessionId)
{
	if (isInvalidate() || m_cgcParser.get() == NULL) return -1;

	unsigned short seq = 0;
	if (m_pResponseHandler)
		seq = m_pResponseHandler->onGetNextSeq();
	//m_bResponseSended = true;
	const tstring responseData = m_cgcParser->getSessionResult(retCode, sSessionId, seq, true);
	if (m_pResponseHandler)
		m_pResponseHandler->onAddSeqInfo((const unsigned char *)responseData.c_str(), responseData.size(), seq, m_cgcParser->getCallid(), m_cgcParser->getSign());

//#ifdef _UNICODE
//	std::string pOutTemp = cgcString::W2Char(responseData);
//	return m_cgcRemote->sendData((const unsigned char*)pOutTemp.c_str(), pOutTemp.length());
//#else
	return m_cgcRemote->sendData((const unsigned char*)responseData.c_str(), responseData.length());
//#endif // _UNICODE
}

int CSotpResponseImpl::sendAppCallResult(long retCode, unsigned long sign, bool bNeedAck)
{
	if (isInvalidate() || m_cgcParser.get() == NULL)
	{
		if (m_pSendLock!=NULL)
		{
			boost::mutex::scoped_lock * pSendLockTemp = m_pSendLock;
			m_pSendLock = NULL;
			delete pSendLockTemp;
		}
		return -1;
	}
	if (m_bNotResponse)
	{
		if (m_pSendLock!=NULL)
		{
			boost::mutex::scoped_lock * pSendLockTemp = m_pSendLock;
			m_pSendLock = NULL;
			delete pSendLockTemp;
		}
		//if (m_session.get() != NULL)
		//{
		//	CSessionImpl* pSessionImpl = (CSessionImpl*)m_session.get();
		//	if (m_nHoldSecond > 0)
		//		pSessionImpl->HoldResponse(this->shared_from_this(), m_nHoldSecond);
		//	//else if (m_nHoldSecond == 0)
		//	//{
		//	//	m_nHoldSecond = -1;
		//	//	pSessionImpl->removeResponse(getRemoteId());
		//	//}
		//}
		return 0;
	}else if (m_nHoldSecond > 0)
	{
		if (m_session.get() != NULL)
		{
			CSessionImpl* pSessionImpl = (CSessionImpl*)m_session.get();
			m_nHoldSecond = -1;
			pSessionImpl->removeResponse(getRemoteId());
		}
	}

	if (sign == 0 && m_cgcParser->getSign() != m_originalCallSign)
	{
		//m_cgcParser->setCallid(m_originalCallId);
		m_cgcParser->setSign(m_originalCallSign);
	}else if (sign != 0 && sign != m_cgcParser->getSign())
	{
		//m_cgcParser->setCallid(0);
		m_cgcParser->setSign(sign);
	}
	unsigned short seq = 0;
	if (m_pResponseHandler)
		seq = m_pResponseHandler->onGetNextSeq();
	//printf("============ seq=%d, handler=%d, needack=%d\n", seq, (int)m_pResponseHandler, bNeedAck?1:0);

	boost::mutex::scoped_lock * pSendLockTemp = m_pSendLock;
	m_pSendLock = NULL;
	m_bResponseSended = true;
	const std::string responseData = m_cgcParser->getAppCallResult(retCode, seq, bNeedAck);
	if (m_cgcParser->isResHasAttachInfo())
	{
		unsigned int nAttachSize = 0;
		unsigned char * pAttachData = m_cgcParser->getResAttachString(nAttachSize);
		//unsigned char * pAttachData = m_cgcParser->getAttachString(m_cgcParser->getResAttachment(), nAttachSize);
		if (pAttachData != NULL)
		{
			unsigned char * pSendData = new unsigned char[nAttachSize+responseData.size()+1];
			memcpy(pSendData, responseData.c_str(), responseData.size());
			memcpy(pSendData+responseData.size(), pAttachData, nAttachSize);
			pSendData[nAttachSize+responseData.size()] = '\0';

			// *避免重
			//m_cgcParser->getResAttachment()->clear();

			int ret = -1;
			if (bNeedAck && m_pResponseHandler != NULL)
				ret = m_pResponseHandler->onAddSeqInfo(pSendData, nAttachSize+responseData.size(), seq, m_cgcParser->getCallid(), m_cgcParser->getSign());

			size_t sendSize = m_cgcRemote->sendData(pSendData, nAttachSize+responseData.size());
			if (pSendLockTemp)
				delete pSendLockTemp;
			delete[] pAttachData;
			if (ret != 0)
				delete[] pSendData;
			return sendSize != nAttachSize+responseData.size() ? 0 : 1;
		}
	}

	if (bNeedAck && m_pResponseHandler != NULL)
		m_pResponseHandler->onAddSeqInfo((const unsigned char *)responseData.c_str(), responseData.size(), seq, m_cgcParser->getCallid(), m_cgcParser->getSign());
	size_t sendSize = m_cgcRemote->sendData((const unsigned char*)responseData.c_str(), responseData.size());
	if (pSendLockTemp)
		delete pSendLockTemp;
	return sendSize;
}
int CSotpResponseImpl::sendP2PTry(void)
{
	if (isInvalidate() || m_cgcParser.get() == NULL) return -1;
	const std::string responseData = m_cgcParser->getP2PTry();
	return m_cgcRemote->sendData((const unsigned char*)responseData.c_str(), responseData.size());
}

void CSotpResponseImpl::setCgcRemote(const cgcRemote::pointer& pcgcRemote)
{
	m_cgcRemote = pcgcRemote;
}

void CSotpResponseImpl::setCgcParser(const cgcParserSotp::pointer& pcgcParser)
{
	m_cgcParser = pcgcParser;
	BOOST_ASSERT (m_cgcParser.get() != NULL);

	// ??
	if (m_cgcParser.get() != 0)
	{
		m_originalCallId = m_cgcParser->getCallid();
		m_originalCallSign = m_cgcParser->getSign();
	}else
	{
		m_originalCallId = 0;
		m_originalCallSign = 0;
	}
}

void CSotpResponseImpl::lockResponse(void)
{
	if (m_bNotResponse)
		return;
	boost::mutex::scoped_lock * pLockTemp = new boost::mutex::scoped_lock(m_sendMutex);
	m_pSendLock = pLockTemp;
}

int CSotpResponseImpl::sendResponse(long retCode, unsigned long sign, bool bNeedAck)
{
	try
	{
		return sendAppCallResult(retCode, sign, bNeedAck);
	}catch (const std::exception &)
	{
	}catch (...)
	{
	}

	return -105;
}

unsigned long CSotpResponseImpl::setNotResponse(int nHoldSecond)
{
	m_bNotResponse = true;
	if (m_nHoldSecond > 0  || nHoldSecond > 0)
	{
		m_nHoldSecond = nHoldSecond;
		if (m_session.get() != NULL)
		{
			CSessionImpl* pSessionImpl = (CSessionImpl*)m_session.get();
			if (m_nHoldSecond <= 0)
			{
				m_nHoldSecond = -1;
				pSessionImpl->removeResponse(getRemoteId());
			}else
			{
				pSessionImpl->HoldResponse(this->shared_from_this(), m_nHoldSecond);
			}
		}
	}
	//if (m_session.get() != NULL)
	//{
	//	CSessionImpl* pSessionImpl = (CSessionImpl*)m_session.get();
	//	pSessionImpl->HoldResponse(shared_from_this(), nHoldSecond);
	//}
	return getRemoteId();
}

void CSotpResponseImpl::invalidate(void)
{
	if (m_cgcRemote.get() != NULL )
		m_cgcRemote->invalidate();
}

/*
int CSotpResponseImpl::sendString(const tstring & sString)
{
	if (isInvalidate()) return -1;
	if (sString.empty()) return 0;

	m_bResponseSended = true;
	tstring responseData = sString;

	try
	{
#ifdef _UNICODE
		std::string pOutTemp = cgcString::W2Char(responseData);
		return m_cgcRemote->sendData((const unsigned char*)pOutTemp.c_str(), pOutTemp.length());
#else
		return m_cgcRemote->sendData((const unsigned char*)responseData.c_str(), responseData.length());
#endif // _UNICODE
	}catch (const std::exception &)
	{
	}catch (...)
	{
	}

	return -105;
}
*/

/*
tstring CSotpResponseImpl::GetResponseClusters(const ClusterSvrList & clusters)
{
	//boost::wformat fRV(L"<svr n=\"%s\" h=\"%s\" p=\"%d\" c=\"%s\" r=\"%d\" />");
	boost::wformat fRV(L"<svr n=\"%s\" h=\"%s\" c=\"%s\" r=\"%d\" />");
	tstring result;
	int count = 0;

	ClusterSvrList::const_iterator pIter;
	for (pIter=clusters.begin(); pIter!=clusters.end(); pIter++)
	{
		ClusterSvr * pCluster = *pIter;

		tstring svrName;
		if (m_sXmlEncoding.compare(L"UTF-8") == 0)
		{
#ifdef WIN32
// ???			svrName = cgcString::Convert(pCluster->getName(), CP_ACP, CP_UTF8);
			int i=0;
#else
			cgcString::GB2312ToUTF_8(svrName, pCluster->getName().c_str(), pCluster->getName().length());
#endif
		}else
		{
			svrName = pCluster->getName();
		}
		tstring sCluster((fRV%svrName.c_str()%pCluster->getHost().c_str()%pCluster->getCode().c_str()%pCluster->getRank()).str());

		result.append(sCluster);

		//
		// 只返回三个足够实现功能
		//
		if (++count == 3)
			break;
	}

	return result;
}
*/
