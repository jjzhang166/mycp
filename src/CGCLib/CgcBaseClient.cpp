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
#pragma warning(disable:4267 4819 4996)
#endif // WIN32

#include "CgcBaseClient.h"
#include <fstream>
#include <boost/format.hpp>
#ifdef WIN32
#include <tchar.h>
#endif // WIN32

namespace cgc
{
#ifdef _UNICODE
typedef boost::wformat tformat;
#else
typedef boost::format tformat;
#endif // _UNICODE

//CXmlParse theXmlParse;

CgcBaseClient::CgcBaseClient(const tstring & clientType)
: m_tSendRecv(0)
, m_pHandler(NULL)
, m_clientState(Init_Client)
, m_clientType(clientType)
, m_bDisableSotpparser(false)
, m_pSendLock(NULL)
//, m_tLastCid(0)
, m_threadActiveSes(NULL), m_threadCIDTimeout(NULL)
, m_nActiveWaitSeconds(0),m_nSendP2PTrySeconds(0)
, m_timeoutSeconds(3), m_timeoutResends(5)
, m_currentPath(_T(""))

{
	m_nDataIndex = 0;
	//memset(&m_pReceiveCidMasks,-1,sizeof(m_pReceiveCidMasks));
}

CgcBaseClient::~CgcBaseClient(void)
{
	StopClient(true);

	//
	// clear client info
	ClearClientInfo();
}

int CgcBaseClient::ParseString(const char * lpszString, const char * lpszInterval, std::vector<std::string> & pOut)
{
	std::string sIn(lpszString);
	const size_t nIntervalLen = strlen(lpszInterval);
	pOut.clear();
	while (!sIn.empty())
	{
		std::string::size_type find = sIn.find(lpszInterval);
		if (find == std::string::npos)
		{
			pOut.push_back(sIn);
			break;
		}
		if (find==0)
			pOut.push_back("");	// 空
		else
			pOut.push_back(sIn.substr(0, find));
		sIn = sIn.substr(find+nIntervalLen);
	}
	return (int)pOut.size();
}
std::string CgcBaseClient::GetHostIp(const char * lpszHostName,const char* lpszDefault)
{
	struct hostent *host_entry;
	//struct sockaddr_in addr;
	/* 即要解析的域名或主机名 */
	host_entry=gethostbyname(lpszHostName);
	//printf("%s\n", dn_or_ip);
	char lpszIpAddress[50];
	memset(lpszIpAddress, 0, sizeof(lpszIpAddress));
	if(host_entry!=0)
	{
		//printf("解析IP地址: ");
		sprintf(lpszIpAddress, "%d.%d.%d.%d",
			(host_entry->h_addr_list[0][0]&0x00ff),
			(host_entry->h_addr_list[0][1]&0x00ff),
			(host_entry->h_addr_list[0][2]&0x00ff),
			(host_entry->h_addr_list[0][3]&0x00ff));
		return lpszIpAddress;
	}else
	{
		return lpszDefault;
	}
}

void CgcBaseClient::do_proc_CgcClient(CgcBaseClient * cgcClient)
{
	if (NULL == cgcClient) return;
	const size_t constMaxSize = 1024*50;
	unsigned char* buffer = new unsigned char [constMaxSize];

	while (!cgcClient->isClientState(CgcBaseClient::Stop_Client))
//	while (!cgcClient->isInvalidate())
	{
		memset(buffer, 0, constMaxSize);
		try
		{
			cgcClient->RecvData(buffer, constMaxSize);
		}catch(const std::exception &)
		{
			//int i=0;
		}catch(...)
		{
			//int i=0;
		}
#ifdef WIN32
		Sleep(5);
#else
		usleep(5000);
#endif
	}
	delete[] buffer;
}

void CgcBaseClient::do_proc_activesession(CgcBaseClient::pointer cgcClient)
{
	BOOST_ASSERT (cgcClient.get() != NULL);

	while (!cgcClient->isInvalidate())
	{
#ifdef WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
		//if (!cgcClient->isTimeToActiveSes()) continue;
		//if (cgcClient->getSessionId().empty()) continue;	// 未打开，或者已经关闭SESSION
		try
		{
			if (cgcClient->isTimeToActiveSes() && !cgcClient->getSessionId().empty())
				cgcClient->sendActiveSession();
			if (cgcClient->isTimeToSendP2PTry())	// 一般发完sendActiveSession，不需要再发sendP2PTry
				cgcClient->sendP2PTry(2);
		}catch(const std::exception &)
		{
		}catch(...)
		{
		}
	}
}

void CgcBaseClient::do_proc_cid_timeout(CgcBaseClient * cgcClient)
{
	if (NULL == cgcClient) return;

	while (!cgcClient->isClientState(CgcBaseClient::Exit_Client))
//	while (!cgcClient->isInvalidate())
	{
		try
		{
			if (!cgcClient->checkSeqTimeout())
			{
#ifdef WIN32
				Sleep(500);
#else
				usleep(500000);
#endif
			}else
			{
#ifdef WIN32
				Sleep(10);
#else
				usleep(10000);
#endif
			}
		}catch(const std::exception &)
		{
		}catch(...)
		{
		}
	}
}

int CgcBaseClient::WSAInit(void)
{
#ifdef WIN32
	WSADATA wsaData;
	int err = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
	if ( err != 0 ) {
//		ACE_ERROR_RETURN ((LM_ERROR, ACE_TEXT("(%P|%t) [ERROR]\t%p\n"), ACE_TEXT("WSAStartup")), err);
	}
#endif
	return 0;
}

void CgcBaseClient::WSAExit(void)
{
#ifdef WIN32
	WSACleanup();
#endif
}

bool CgcBaseClient::setClientPath(const tstring & sClientPath)
{
	m_currentPath = sClientPath;

	if (sClientPath.empty())
	{
		namespace fs = boost::filesystem;
#ifdef _UNICODE
		fs::wpath currentPath(fs::initial_path<fs::wpath>());
#else
		fs::path currentPath(fs::initial_path());
#endif
		m_currentPath = currentPath.string();
	}
	//tstring 
	tstring clientFullPath(m_currentPath);
	clientFullPath.append(_T("/ss_client_info"));

	//
	// open file
	tfstream fsClientPath;
#ifdef WIN32
	//fsClientPath.open(clientFullPath.c_str(), std::ios::in|std::ios::_Nocreate);
	fsClientPath.open(clientFullPath.c_str(), std::ios::in);
#else
	fsClientPath.open(clientFullPath.c_str(), std::ios::in);
#endif
	if (!fsClientPath.is_open()) return false;

	//
	// clear client info first.
	ClearClientInfo();

	//
	// load information
	Serialize(false, fsClientPath);

	//
	// close file
	fsClientPath.close();
	return true;
}

void CgcBaseClient::setCIDTResends(unsigned int timeoutResends, unsigned int timeoutSeconds)
{
	this->m_timeoutResends = timeoutResends;
	this->m_timeoutSeconds = timeoutSeconds;
}

int CgcBaseClient::StartClient(const tstring & sCgcServerAddr, unsigned int bindPort)
{
	// 
	// is already start
	if (isStarted()) return 0;

	WSAInit();

	int ret = 0;
	try
	{
		ret = startClient(sCgcServerAddr, bindPort);
	}catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}catch(...)
	{
		std::cerr << "startClient exception." << std::endl;
		return -1;
	}
	if (ret != 0)
		return ret;

	//
	// start the CID timeout process thread
	if (m_threadCIDTimeout == NULL)
	{
		boost::thread_attributes attrs;
		attrs.set_stack_size(10240);	// 10K
		m_threadCIDTimeout = new boost::thread(attrs,boost::bind(do_proc_cid_timeout, this));
	}

	m_clientState = Start_Client;
	return 0;
}

void CgcBaseClient::StartRecvThreads(unsigned short nRecvThreads)
{
	nRecvThreads = nRecvThreads > 20 ? 20 : nRecvThreads;
	unsigned short i=0;
	for (i=0; i<nRecvThreads; i++)
	{
		boost::thread_attributes attrs;
		attrs.set_stack_size(CGC_THREAD_STACK_MIN);
		boost::thread * recvThread = new boost::thread(attrs,boost::bind(do_proc_CgcClient, this));
		m_listBoostThread.push_back(recvThread);
	}
}

void CgcBaseClient::StopRecvThreads(void)
{
	BoostThreadListCIter pIter;
	for (pIter=m_listBoostThread.begin(); pIter!=m_listBoostThread.end(); pIter++)
	{
		boost::thread * recvThread = *pIter;
		recvThread->join();	// 如果该线程在外面操作界面消息，会导致退出挂死
		delete recvThread;
	}
	m_listBoostThread.clear();
}

void CgcBaseClient::StartActiveThread(unsigned short nActiveWaitSeconds,unsigned short nSendP2PTrySeconds)
{
	m_nActiveWaitSeconds = nActiveWaitSeconds;
	m_nSendP2PTrySeconds = nSendP2PTrySeconds;
	if ((m_nActiveWaitSeconds > 0 || m_nSendP2PTrySeconds>0) && m_threadActiveSes == NULL)
	{
		boost::thread_attributes attrs;
		attrs.set_stack_size(10240);	// 10K
		m_threadActiveSes = new boost::thread(boost::bind(do_proc_activesession, shared_from_this()));
	}
}

void CgcBaseClient::StopActiveThread(void)
{
	if (m_threadActiveSes)
	{
		m_threadActiveSes->join();
		delete m_threadActiveSes;
		m_threadActiveSes = NULL;
	}
	m_nActiveWaitSeconds = 0;
	m_nSendP2PTrySeconds = 0;
}

int CgcBaseClient::NextCluster(void)
{
	// ? SOTP/2.0
/*	if (m_custerSvrList.empty()) return -1;

	m_custerSvrList.push_back(m_custerSvrList.front());
	m_custerSvrList.pop_front();
*/
	return 0;
}

void CgcBaseClient::StopClient(bool exitClient)
{
	if (!isStarted()) return;

	m_mapSeqInfo.clear();

	m_clientState = Stop_Client;
	if (m_pSendLock)
	{
		boost::mutex::scoped_lock * pLockTemp = m_pSendLock;
		m_pSendLock = NULL;
		delete pLockTemp;
	}

	sendCloseSession();

	try
	{
		stopClient();
	}catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}catch(...)
	{
	}

	// clear m_listBoostThread
	StopRecvThreads();

	// stop the active session thread
	StopActiveThread();

	//
	// save client info
	SaveClientInfo();

	// clear info
	m_sSessionId.clear();
	RemoveAllCidData();

	if (exitClient)
	{
		// for exit m_threadCIDTimeout thread.
		m_clientState = Exit_Client;

		// stop the CID timeout process thread
		if (m_threadCIDTimeout)
		{
			m_threadCIDTimeout->join();
			delete m_threadCIDTimeout;
			m_threadCIDTimeout = NULL;
		}
	}

	// clear m_mapCidInfo
	m_ipLocal.reset();
	m_ipRemote.reset();

	m_pReceiveInfoList.clear();
	//memset(&m_pReceiveCidMasks,-1,sizeof(m_pReceiveCidMasks));
}

int CgcBaseClient::sendQueryClusterSvr(const tstring & sAccount, const tstring & sPasswd, unsigned long * pCallId)
{
	if (this->isInvalidate()) return -1;

	// ???
	return 0;
}

int CgcBaseClient::sendVerifyClusterSvr(unsigned long * pCallId)
{
	if (this->isInvalidate()) return -1;
	//tformat fVefiryCluster(_T("<sotp.clu type=\"verify\" cid=\"%d\" value=\"%s\" />"));

	// ???
	return 0;
}

bool CgcBaseClient::sendOpenSession(unsigned long * pCallId)
{
	if (this->isInvalidate()) return false;

	// is already opened
	if (!m_sSessionId.empty()) return true;

	// cid
	unsigned long cid = getNextCallId();
	if (pCallId)
		*pCallId = cid;
	const unsigned short seq = getNextSeq();
	// requestData
	std::string requestData = toOpenSesString(cid, seq, true);
	// addSeqInfo
	addSeqInfo((const unsigned char*)requestData.c_str(), requestData.size(), seq, cid);
	// sendData
	try
	{
		sendData((const unsigned char*)requestData.c_str(), requestData.size());
		//return (sendSize == requestData.size()) ? 0 : 1;
		return true;
	}catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}catch(...)
	{}
	return false;
}

void CgcBaseClient::sendCloseSession(unsigned long * pCallId)
{
	if (this->isInvalidate()) return;

	// is already closed
	if (m_sSessionId.empty()) return;

	// cid
	unsigned long cid = getNextCallId();
	if (pCallId)
		*pCallId = cid;
	const unsigned short seq = getNextSeq();
	// requestData
	std::string requestData = toSesString(SotpCallTable2::PT_Close, m_sSessionId, cid, seq, true);
	// addSeqInfo
	addSeqInfo((const unsigned char*)requestData.c_str(), requestData.size(), seq, cid);
	// sendData
	sendData((const unsigned char*)requestData.c_str(), requestData.size());
	// ?
	m_sSessionId = _T("");
}

void CgcBaseClient::sendActiveSession(unsigned long * pCallId)
{
	if (this->isInvalidate()) return;

	// is closed state
	if (m_sSessionId.empty()) return;

	// cid
	unsigned long cid = getNextCallId();
	if (pCallId)
		*pCallId = cid;
	const unsigned short seq = getNextSeq();
	// requestData
	std::string requestData = toSesString(SotpCallTable2::PT_Active, m_sSessionId, cid, seq, true);
	// addSeqInfo
	addSeqInfo((const unsigned char*)requestData.c_str(), requestData.size(), seq, cid,0,SotpCallTable2::PT_Active);
	// sendData
	sendData((const unsigned char*)requestData.c_str(), requestData.size());
}
void CgcBaseClient::sendP2PTry(unsigned short nTryCount)
{
	if (this->isInvalidate()) return;
	// requestData
	const std::string requestData = toP2PTry();
	// sendData
	for (unsigned short i=0; i<nTryCount; i++)
	{
		sendData((const unsigned char*)requestData.c_str(), requestData.size());
		if (nTryCount>1)
#ifdef WIN32
			Sleep(1);
#else
			usleep(1000);
#endif
	}
}

void CgcBaseClient::addSeqInfo(const unsigned char * callData, unsigned int dataSize, unsigned short seq, unsigned long cid, unsigned long sign,unsigned int nUserData)
{
	if (m_timeoutResends <= 0 || m_timeoutSeconds <= 0) return;
	if (callData == 0 || dataSize == 0) return;

	cgcSeqInfo::pointer pSeqInfo;
	if (m_mapSeqInfo.find(seq, pSeqInfo))
	{
		pSeqInfo->setCallData(callData, dataSize);
		pSeqInfo->setTimeoutResends(m_timeoutResends);
		pSeqInfo->setTimeoutSeconds(m_timeoutSeconds);
		pSeqInfo->setSign(sign);
		pSeqInfo->setUserData(nUserData);
		pSeqInfo->setSendTime();
	}else
	{
		pSeqInfo = cgcSeqInfo::create(seq, cid, sign, m_timeoutResends, m_timeoutSeconds);
		pSeqInfo->setCallData(callData, dataSize);
		pSeqInfo->setUserData(nUserData);
		m_mapSeqInfo.insert(seq, pSeqInfo);
	}
	pSeqInfo->setSessionId(this->m_sSessionId);
	pSeqInfo->SetAddress(m_ipRemote.address());

	//CidInfo * pCidInfo = m_mapSeqInfo.find(cid, false);
	//if (pCidInfo == 0)
	//{
	//	pCidInfo = new CidInfo(cid, sign, m_timeoutResends, m_timeoutSeconds);
	//	pCidInfo->setCallData(callData, dataSize);
	//	m_mapCidInfo.insert(cid, pCidInfo);
	//}else
	//{
	//	pCidInfo->setCallData(callData, dataSize);
	//	pCidInfo->setTimeoutResends(m_timeoutResends);
	//	pCidInfo->setTimeoutSeconds(m_timeoutSeconds);
	//	pCidInfo->setSign(sign);
	//	pCidInfo->setSendTime();
	//}
	//pCidInfo->setSessionId(this->m_sSessionId);
}

bool CgcBaseClient::addSeqInfo(unsigned char * callData, unsigned int dataSize, unsigned short seq, unsigned long cid, unsigned long sign,unsigned int nUserData)
{
	if (m_timeoutResends <= 0 || m_timeoutSeconds <= 0) return false;
	if (callData == 0 || dataSize == 0) return false;

	cgcSeqInfo::pointer pSeqInfo;
	if (m_mapSeqInfo.find(seq, pSeqInfo))
	{
		pSeqInfo->setCallData(callData, dataSize);
		pSeqInfo->setTimeoutResends(m_timeoutResends);
		pSeqInfo->setTimeoutSeconds(m_timeoutSeconds);
		pSeqInfo->setSign(sign);
		pSeqInfo->setUserData(nUserData);
		pSeqInfo->setSendTime();
	}else
	{
		pSeqInfo = cgcSeqInfo::create(seq, cid, sign, m_timeoutResends, m_timeoutSeconds);
		pSeqInfo->setCallData(callData, dataSize);
		pSeqInfo->setUserData(nUserData);
		m_mapSeqInfo.insert(seq, pSeqInfo);
	}
	pSeqInfo->setSessionId(this->m_sSessionId);
	pSeqInfo->SetAddress(m_ipRemote.address());

	//CidInfo * pCidInfo = m_mapCidInfo.find(cid, false);
	//if (pCidInfo == 0)
	//{
	//	pCidInfo = new CidInfo(cid, sign, m_timeoutResends, m_timeoutSeconds);
	//	pCidInfo->setCallData(callData, dataSize);
	//	m_mapCidInfo.insert(cid, pCidInfo);
	//}else
	//{
	//	pCidInfo->setCallData(callData, dataSize);
	//	pCidInfo->setTimeoutResends(m_timeoutResends);
	//	pCidInfo->setTimeoutSeconds(m_timeoutSeconds);
	//	pCidInfo->setSign(sign);
	//	pCidInfo->setSendTime();
	//}
	//pCidInfo->setSessionId(this->m_sSessionId);
	return true;
}

void CgcBaseClient::beginCallLock(void)
{
	boost::mutex::scoped_lock * pLockTemp = new boost::mutex::scoped_lock(m_sendMutex);
	m_pSendLock = pLockTemp;
}

//int CgcBaseClient::sendAppCall(unsigned long nCallSign, const tstring & sCallName, const tstring & sAppName, const Attachment * pAttach, unsigned long * pCallId)
bool CgcBaseClient::sendAppCall(unsigned long nCallSign, const tstring & sCallName, bool bNeedAck,const cgcAttachment::pointer& pAttach, unsigned long * pCallId)
{
	boost::mutex::scoped_lock * pLockTemp = m_pSendLock;
	m_pSendLock = NULL;
	if (sCallName.empty() || this->isInvalidate())
	{
		if (pLockTemp)
			delete pLockTemp;
		return false;
	}
	unsigned long cid = getNextCallId();
	if (pCallId)
		*pCallId = cid;
	//const unsigned short seq = getNextSeq();
	const unsigned short seq = bNeedAck?getNextSeq():0;

	const std::string requestData = toAppCallString(cid,nCallSign,sCallName,seq,bNeedAck);

	// sendData
	if (pAttach.get() != NULL)
	{
		unsigned int nAttachSize = 0;
		unsigned char * pAttachData = toAttachString(pAttach, nAttachSize);
		if (pAttachData != NULL)
		{
			unsigned char * pSendData = new unsigned char[nAttachSize+requestData.size()+1];
			memcpy(pSendData, requestData.c_str(), requestData.size());
			memcpy(pSendData+requestData.size(), pAttachData, nAttachSize);
			pSendData[nAttachSize+requestData.size()] = '\0';

			// addSeqInfo
			bool ret = false;
			if (bNeedAck)
				ret = addSeqInfo(pSendData, nAttachSize+requestData.size(), seq, cid, nCallSign);

			if (pLockTemp)
				delete pLockTemp;
			sendData(pSendData, nAttachSize+requestData.size());
			delete[] pAttachData;
			if (!ret)
				delete[] pSendData;
			//return sendSize != nAttachSize+requestData.size() ? 0 : 1;
			return true;
		}
	}

	// addSeqInfo
	if (bNeedAck)
	{
		addSeqInfo((const unsigned char*)requestData.c_str(), requestData.size(), seq, cid, nCallSign);
	}
	if (pLockTemp)
		delete pLockTemp;

	sendData((const unsigned char*)requestData.c_str(), requestData.size());
	//return sendSize != requestData.size() ? 0 : 1;
	return true;
}
bool CgcBaseClient::sendCallResult(long nResult,unsigned long nCallId,unsigned long nCallSign,bool bNeedAck,const cgcAttachment::pointer& pAttach)
{
	boost::mutex::scoped_lock * pLockTemp = m_pSendLock;
	m_pSendLock = NULL;
	if (this->isInvalidate())
	{
		if (pLockTemp)
			delete pLockTemp;
		return false;
	}
	//const unsigned short seq = getNextSeq();
	const unsigned short seq = bNeedAck?getNextSeq():0;

	const std::string requestData = toAppCallResult(nCallId,nCallSign,nResult,seq,bNeedAck);

	// sendData
	if (pAttach.get() != NULL)
	{
		unsigned int nAttachSize = 0;
		unsigned char * pAttachData = toAttachString(pAttach, nAttachSize);
		if (pAttachData != NULL)
		{
			unsigned char * pSendData = new unsigned char[nAttachSize+requestData.size()+1];
			memcpy(pSendData, requestData.c_str(), requestData.size());
			memcpy(pSendData+requestData.size(), pAttachData, nAttachSize);
			pSendData[nAttachSize+requestData.size()] = '\0';

			// addSeqInfo
			bool ret = false;
			if (bNeedAck)
				ret = addSeqInfo(pSendData, nAttachSize+requestData.size(), seq, nCallId, nCallSign);

			if (pLockTemp)
				delete pLockTemp;
			sendData(pSendData, nAttachSize+requestData.size());
			delete[] pAttachData;
			if (!ret)
				delete[] pSendData;
			//return sendSize != nAttachSize+requestData.size() ? 0 : 1;
			return true;
		}
	}

	// addSeqInfo
	if (bNeedAck)
	{
		addSeqInfo((const unsigned char*)requestData.c_str(), requestData.size(), seq, nCallId, nCallSign);
	}
	if (pLockTemp)
		delete pLockTemp;

	sendData((const unsigned char*)requestData.c_str(), requestData.size());
	return true;
}

void * CgcBaseClient::SetCidData(unsigned long cid, void * pData)
{
	boost::mutex::scoped_lock lock(m_mutexCidPtrMap);

	void * pResult = NULL;	
	ULongPtrMap::iterator pIter = m_mapCidPtr.find(cid);
	if (pIter != m_mapCidPtr.end())
	{
		pResult = pIter->second;
		m_mapCidPtr[cid] = pData;
	}else
	{
		m_mapCidPtr.insert(ULongPtrPair(cid, pData));
	}

	return pResult;
}

void * CgcBaseClient::GetCidData(unsigned long cid)
{
	boost::mutex::scoped_lock lock(m_mutexCidPtrMap);
	ULongPtrMap::iterator pIter = m_mapCidPtr.find(cid);
	return pIter != m_mapCidPtr.end() ? pIter->second : NULL;
}

void * CgcBaseClient::RemoveCidData(unsigned long cid)
{
	boost::mutex::scoped_lock lock(m_mutexCidPtrMap);
	void * pResult = NULL;
	ULongPtrMap::iterator pIter = m_mapCidPtr.find(cid);
	if (pIter != m_mapCidPtr.end())
	{
		pResult = pIter->second;
		m_mapCidPtr.erase(pIter);
	}

	return pResult;
}

void CgcBaseClient::RemoveAllCidData(void)
{
	boost::mutex::scoped_lock lock(m_mutexCidPtrMap);
	m_mapCidPtr.clear();
}

void CgcBaseClient::RemoveAllCidData(ULongPtrMap &mapCidPtr)
{
	boost::mutex::scoped_lock lock(m_mutexCidPtrMap);
	ULongPtrMap::iterator pIter;
	for (pIter=m_mapCidPtr.begin(); pIter!=m_mapCidPtr.end(); pIter++)
	{
		mapCidPtr.insert(ULongPtrPair(pIter->first, pIter->second));
	}
	m_mapCidPtr.clear();
}

tstring CgcBaseClient::getLocaleAddr(u_short & portOut) const
{
	portOut = m_ipLocal.getport();
	return m_ipLocal.address();
}

tstring CgcBaseClient::getRemoteAddr(void) const
{
	return m_ipRemote.address();
}

size_t CgcBaseClient::RecvData(unsigned char * buffer, size_t size)
{
	if (isInvalidate()) return 0;
	if (buffer == 0) return -1;

	boost::mutex::scoped_lock lock(m_recvMutex);

	// virtual
	size_t recvSize = 0;
	try
	{
		recvSize = recvData(buffer, size);
	}catch (std::exception& e)
	{
		printf("!!!! recvData exception: %s\n",e.what());
		return -2;
	}catch(...)
	{
		return -2;
	}

	if (recvSize <= 0 || strlen((const char*)buffer) == 0) return 0;

	m_tSendRecv = time(0);
	lock.unlock();
	
	try
	{
		parseData(CCgcData::create(buffer, recvSize),0);
	}catch (std::exception& e)
	{
		//if (m_pHandler)
		//	m_pHandler->OnCgcResponse(recvData);
		printf("!!!! parseData exception: %s\n",e.what());
		return -2;
	}catch(...)
	{
		return -2;
	}

	return recvSize;
}

void CgcBaseClient::parseData(const CCgcData::pointer& recvData,unsigned long nRemoteId)
{
	BOOST_ASSERT (recvData.get() != NULL);

	if (m_bDisableSotpparser && m_pHandler != NULL)
	{
		m_pHandler->OnCgcResponse(recvData);
		return;
	}
	//if (strstr((const char*)recvData->data(),"p2ptry")!=NULL)
	//{
	//	int i=0;
	//}

	CPPSotp2 ppSotp;
	if (ppSotp.doParse(recvData->data(), recvData->size(),doGetEncoding().c_str()))
	{
		if (ppSotp.isP2PProto()) return;
		if (ppSotp.hasSeq())
		{
			const short seq = ppSotp.getSeq();
			if (ppSotp.isAckProto())
			{
				m_mapSeqInfo.remove(seq);
				return;
			}
			if (ppSotp.isNeedAck())
			{
				const std::string requestData = toAckString(seq);
				sendData((const unsigned char*)requestData.c_str(), requestData.size());
			}
			CReceiveInfo::pointer pReceiveInfo;
			if (!m_pReceiveInfoList.find(nRemoteId,pReceiveInfo))
			{
				pReceiveInfo = CReceiveInfo::create();
				m_pReceiveInfoList.insert(nRemoteId,pReceiveInfo);
			}
			boost::mutex::scoped_lock lock(pReceiveInfo->m_recvSeq);
			if (pReceiveInfo->m_tLastCid>0 && (m_tSendRecv-pReceiveInfo->m_tLastCid) > 15)	// 10
			{
				memset(&pReceiveInfo->m_pReceiveCidMasks,-1,sizeof(pReceiveInfo->m_pReceiveCidMasks));
			}else
			{
				for (int i=0; i<MAX_CID_MASKS_SIZE; i++)
				{
					// Receive duplation message, 
					if (pReceiveInfo->m_pReceiveCidMasks[i] == seq)
					{
						//m_tLastCid = m_tSendRecv;
						return;
					}
				}
			}
			pReceiveInfo->m_tLastCid = m_tSendRecv;
			pReceiveInfo->m_nDataIndex++;
			pReceiveInfo->m_pReceiveCidMasks[pReceiveInfo->m_nDataIndex%MAX_CID_MASKS_SIZE] = seq;

			//boost::mutex::scoped_lock lock(m_recvSeq);
			//if (m_tLastCid>0 && (m_tSendRecv-m_tLastCid) > 15)	// 10
			//{
			//	memset(&m_pReceiveCidMasks,-1,sizeof(m_pReceiveCidMasks));
			//}else
			//{
			//	// nRemoteId
			//	for (int i=0; i<MAX_CID_MASKS_SIZE; i++)
			//	{
			//		// Receive duplation message, 
			//		if (m_pReceiveCidMasks[i] == seq)
			//		{
			//			//m_tLastCid = m_tSendRecv;
			//			return;
			//		}
			//	}
			//}
			//m_tLastCid = m_tSendRecv;
			//m_nDataIndex++;
			//m_pReceiveCidMasks[m_nDataIndex%MAX_CID_MASKS_SIZE] = seq;
		}

		int nResultValue = 0;
		if (ppSotp.isResulted())
		{
			if (ppSotp.isSessionProto())
			{
				// session protocol
				if (ppSotp.isOpenType())
				{
					m_sSessionId = ppSotp.getSid();

					//
					// save client info
					this->SaveClientInfo();
				}else if (ppSotp.isCloseType())
					m_sSessionId.clear();

/*			}else if (ppSotp.isClusterProto())
			{
				//
				// cluster protocol
				if (ppSotp.isQueryType())
				{
					// ? SOTP/2.0
					// clear m_custerSvrList
					//for_each(m_custerSvrList.begin(), m_custerSvrList.end(), DeletePtr());
					//m_custerSvrList.clear();

					//
					// getOutClusters
// ???					parseResponse.getClusters(this->m_custerSvrList);
				}*/
			}

//			nResultValue = parseResponse.getResultValue();
			nResultValue = ppSotp.getResultValue();
		}

		if (nResultValue == -103)
			m_sSessionId.clear();
		else if (nResultValue == -117)
			m_mapSeqInfo.clear();

		//
		// fire the event
		if (m_pHandler)
			m_pHandler->OnCgcResponse(ppSotp);

		// '-103': invalidate session handle
		//if (ppSotp.isActiveType() && nResultValue == -103)
		//	m_sSessionId.clear();
	}else
	{
		if (m_pHandler)
			m_pHandler->OnCgcResponse(recvData);
	}
}

//size_t CgcBaseClient::SendData(const unsigned char * data, size_t size)
//{
//	m_tSendRecv = time(0);
//	return sendData(data, size);
//}

bool CgcBaseClient::isTimeToActiveSes(void) const
{
	if (m_tSendRecv == 0 || m_nActiveWaitSeconds==0)
		return false;
	const time_t tNow = time(0);
	if ((tNow > (time_t)(m_tSendRecv+m_nActiveWaitSeconds)) ||
		(tNow < m_tSendRecv))	// has change the machine time
	{
		return true;
	}
	return false;
}
bool CgcBaseClient::isTimeToSendP2PTry(void) const
{
	if (m_tSendRecv == 0 || m_nSendP2PTrySeconds==0)
		return false;
	const time_t tNow = time(0);
	if ((tNow > (time_t)(m_tSendRecv+m_nSendP2PTrySeconds)) ||
		(tNow < m_tSendRecv))	// has change the machine time
	{
		return true;
	}
	return false;
}

bool CgcBaseClient::checkSeqTimeout(void)
{
	BoostReadLock rdlock(m_mapSeqInfo.mutex());
	CLockMap<unsigned short, cgcSeqInfo::pointer>::iterator pIter;
	for (pIter=m_mapSeqInfo.begin(); pIter!=m_mapSeqInfo.end(); pIter++)
	{
		cgcSeqInfo::pointer pCidInfo = pIter->second;
		if (pCidInfo->isTimeout())
		{
			if (pCidInfo->canResendAgain())
			{
				pCidInfo->increaseResends();
				pCidInfo->setSendTime();
				// resend
				const tstring sAddress = this->getRemoteAddr();
				this->setRemoteAddr(pCidInfo->GetAddress());
				sendData(pCidInfo->getCallData(), pCidInfo->getDataSize());
				this->setRemoteAddr(sAddress);

				// ?
				//if (m_pHandler)
				//	m_pHandler->OnCidTimeout(pCidInfo->getCid(), pCidInfo->getSign(), true);
			}else
			{
				// 
				unsigned short nSeq = pIter->first;
				rdlock.unlock();
				m_mapSeqInfo.remove(nSeq);

				// OnCidResend
				if (m_pHandler)
				{
					if (pCidInfo->getUserData()==SotpCallTable2::PT_Active)
						m_pHandler->OnActiveTimeout();
					else
						m_pHandler->OnCidTimeout(pCidInfo->getCid(), pCidInfo->getSign(), false);
				}
			}
			return true;
		}
	}

	return false;
}

void CgcBaseClient::ClearClientInfo(void)
{
	// ? SOTP/2.0
	// clear m_custerSvrList
//	for_each(m_custerSvrList.begin(), m_custerSvrList.end(), DeletePtr());
//	m_custerSvrList.clear();
}

void CgcBaseClient::SaveClientInfo(void)
{
	if (m_currentPath.empty()) return;

	tstring clientFullPath(m_currentPath);
	clientFullPath.append(_T("/ss_client_info"));

	//
	// open file
	tfstream fsClientPath;
	fsClientPath.open(clientFullPath.c_str(), std::ios::out);
	if (!fsClientPath.is_open()) return;

	//
	// load information
	Serialize(true, fsClientPath);

	//
	// close file
	fsClientPath.close();
}

#ifndef _UNICODE
typedef char TCHAR;
#endif // _UNICODE

void CgcBaseClient::Serialize(bool isStoring, tfstream& ar)
{
	if (!ar.is_open()) return;

	if (isStoring)
	{
		tstring::size_type len = 0;
		//int size = 0;

		// ? SOTP/2.0
		// m_custerSvrList
/*		size = m_custerSvrList.size();
		ar.write((const TCHAR*)(&size), sizeof(size));
		ClusterSvrList::iterator iterClusterSvrList;
		for (iterClusterSvrList=m_custerSvrList.begin(); iterClusterSvrList!=m_custerSvrList.end(); iterClusterSvrList++)
		{
			ClusterSvr * pClusterSvr = *iterClusterSvrList;
			pClusterSvr->Serialize(isStoring, ar);
		}
*/
		// m_sAppName
		len = m_sAppName.length();
		ar.write((const TCHAR*)(&len), sizeof(tstring::size_type));
		ar.write(m_sAppName.c_str(), len);

		// m_sAccount
		len = m_sAccount.length();
		ar.write((const TCHAR*)(&len), sizeof(tstring::size_type));
		ar.write(m_sAccount.c_str(), len);

		// m_sPasswd
		len = m_sPasswd.length();
		ar.write((const TCHAR*)(&len), sizeof(tstring::size_type));
		ar.write(m_sPasswd.c_str(), len);

	}else
	{
		TCHAR * buffer = 0;
		tstring::size_type len = 0;
		//int size = 0;

		// ? SOTP/2.0
		// m_custerSvrList
/*		ar.read((TCHAR*)(&size), sizeof(size));
		for (int i=0; i<size; i++)
		{
			ClusterSvr * pClusterSvr = new ClusterSvr();
			pClusterSvr->Serialize(isStoring, ar);
			m_custerSvrList.push_back(pClusterSvr);
		}
*/
		// m_sAppName
		ar.read((TCHAR*)(&len), sizeof(tstring::size_type));
		buffer = new TCHAR[len+1];
		ar.read(buffer, len);
		buffer[len] = '\0';
		m_sAppName = buffer;
		delete []buffer;

		// m_sAccount
		ar.read((TCHAR*)(&len), sizeof(tstring::size_type));
		buffer = new TCHAR[len+1];
		ar.read(buffer, len);
		buffer[len] = '\0';
		m_sAccount = buffer;
		delete []buffer;

		// m_sPasswd
		ar.read((TCHAR*)(&len), sizeof(tstring::size_type));
		buffer = new TCHAR[len+1];
		ar.read(buffer, len);
		buffer[len] = '\0';
		m_sPasswd = buffer;
		delete []buffer;

	}
}

} // namespace cgc
