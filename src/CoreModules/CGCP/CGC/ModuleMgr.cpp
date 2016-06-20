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
#include <winsock2.h>
#include <Windows.h>
#endif

#include "ModuleMgr.h"
#include <algorithm>
#include <stdarg.h>
using namespace std;

#define USES_SOTP_CLIENT

CModuleImpl::CModuleImpl(void)
: m_callRef(0)
, m_moduleState(0)
, m_nInited(false)
, m_pServiceManager(NULL)
, m_pSyncErrorStoped(false), m_pSyncThreadKilled(false)
, m_tProcSyncHostsTime(0), m_bLoadBackupFromCdbcService(false)
, m_nCurrentCallId(0)

{
}

CModuleImpl::CModuleImpl(const ModuleItem::pointer& module, cgcServiceManager* pServiceManager)
: m_module(module)
, m_callRef(0)
, m_moduleState(0)
, m_nInited(false)
, m_pServiceManager(pServiceManager)
, m_pSyncErrorStoped(false), m_pSyncThreadKilled(false)
, m_tProcSyncHostsTime(0), m_bLoadBackupFromCdbcService(false)
, m_nCurrentCallId(0)

{
}

CModuleImpl::~CModuleImpl(void)
{
	StopModule();
}

void CModuleImpl::StopModule(void)
{
	resetIoService(true);
	m_timerTable.KillAll();
	m_logService.reset();
	m_moduleParams.clearParameters();
	m_attributes.reset();

	if (m_pServiceManager!=NULL && m_pSyncCdbcService.get()!=NULL)
	{
		m_pServiceManager->resetService(m_pSyncCdbcService);
		m_pSyncCdbcService.reset();
	}
	if (m_pSyncThread.get()!=NULL)
	{
		m_pSyncThreadKilled = true;
		m_pSyncThread->join();
		m_pSyncThread.reset();
	}
	m_pSyncErrorStoped = false;
	m_bLoadBackupFromCdbcService = false;
	m_pSyncList.clear();
	m_pHostList.clear();
	m_pSyncSotpClientList.clear();
	m_pModuleSotpClientList.clear();
	m_pSyncRequestList.clear();
}
void CModuleImpl::OnCheckTimeout(void)
{
	// 定时清空没用sotp client数据
	//printf("**** OnCheckTimeout(size=%d)...\n",(int)m_pModuleSotpClientList.size());
	const time_t tNow = time(0);
	int nEraseCount = 0;
	BoostReadLock rdLock(m_pModuleSotpClientList.mutex());
	CLockMap<tstring, mycp::CCGCSotpClient::pointer>::iterator pIter=m_pModuleSotpClientList.begin();
	for (;pIter!=m_pModuleSotpClientList.end();pIter++)
	{
		mycp::CCGCSotpClient::pointer pPOPSotpClient = pIter->second;
		const time_t tSendRecvTime = pPOPSotpClient->sotp()->doGetLastSendRecvTime();
		const  unsigned int nSendReceiveTimeout = pPOPSotpClient->GetUserData();
		//printf("**** OnCheckTimeout lasttime=%lld\n",(cgc::bigint)tSendRecvTime);
		if (tSendRecvTime > 0 && (tSendRecvTime+nSendReceiveTimeout)<tNow)
		{
			// 已经通过 XX 秒没有数据，清空该sotp client
			//printf("**** OnCheckTimeout clear um client\n");
			m_pModuleSotpClientList.erase(pIter);
			if ((++nEraseCount)>=200 || m_pModuleSotpClientList.empty(false))
				break;
			else
				pIter = m_pModuleSotpClientList.begin();
		}
	}
}

tstring CModuleImpl::getAppConfPath(void) const
{
	tstring result(m_sModulePath);
	result.append(_T("/conf/"));
	result.append(getApplicationName());

	return result;
}

long CModuleImpl::getMajorVersion(void) const
{
	return m_moduleParams.getMajorVersion();
}

long CModuleImpl::getMinorVersion(void) const
{
	return m_moduleParams.getMinorVersion();
}

bool CModuleImpl::SetTimer(unsigned int nIDEvent, unsigned int nElapse, cgcOnTimerHandler::pointer handler, bool bOneShot, const void * pvParam)
{
	return m_timerTable.SetTimer(nIDEvent, nElapse, handler, bOneShot, pvParam);
}

void CModuleImpl::KillTimer(unsigned int nIDEvent)
{
	return m_timerTable.KillTimer(nIDEvent);
}

void CModuleImpl::KillAllTimer(void)
{
	return m_timerTable.KillAll();
}

cgcAttributes::pointer CModuleImpl::getAttributes(bool create)
{
	if (create && m_attributes.get() == NULL)
		m_attributes = cgcAttributes::pointer(new AttributesImpl());
	return m_attributes;
}
inline int ParseString(const char * lpszString, const char * lpszInterval, std::vector<tstring> & pOut)
{
	const tstring sIn(lpszString);
	const size_t nInLen = sIn.size();
	const size_t nIntervalLen = strlen(lpszInterval);
	pOut.clear();
	std::string::size_type nFindBegin = 0;
	while (nFindBegin<nInLen)
	{
		std::string::size_type find = sIn.find(lpszInterval,nFindBegin);
		if (find == std::string::npos)
		{
			pOut.push_back(sIn.substr(nFindBegin));
			break;
		}
		if (find==nFindBegin)
		{
			pOut.push_back("");	// 空
		}else
		{
			pOut.push_back(sIn.substr(nFindBegin, (find-nFindBegin)));
		}
		nFindBegin = find+nIntervalLen;
	}
	return (int)pOut.size();
}
inline cgc::bigint GetNextBigId(bool bLongBig=true)	// index++ 16
{
	static unsigned short static_id_index = 0;
	char lpszNextId[24];
	if (bLongBig)
		sprintf(lpszNextId, "%d%04d%04d", 10000000+((int)time(0)%89999999),(++static_id_index)%10000,rand()%10000);
	else
		sprintf(lpszNextId, "%d%03d%03d", 10000000+((int)time(0)%89999999),(++static_id_index)%1000,rand()%1000);
	return cgc_atoi64(lpszNextId);
}
#define USES_PRINT_SYNC_DEBUG

mycp::asio::IoService::pointer CModuleImpl::getIoService(bool bCreateAndStart)
{
	if (m_pIoService.get()==NULL && bCreateAndStart)
	{
		m_pIoService = mycp::asio::IoService::create();
		m_pIoService->start();
	}
	return m_pIoService;
}
void CModuleImpl::resetIoService(bool bStopIoService)
{
	if (m_pIoService.get()!=NULL && bStopIoService)
	{
		m_pIoService->stop();
	}
	m_pIoService.reset();
}

const int max_sync_name_size = 64;
const int max_sync_data_size = 4096;
int CModuleImpl::sendSyncData(const tstring& sSyncName, int nDataType, const char* sDataString, size_t nDataSize, bool bBackup)
{
	if (!m_moduleParams.isSyncEnable() || m_moduleParams.getSyncHosts().empty()) return 0;
	if (sSyncName.size()>max_sync_name_size || nDataSize>max_sync_data_size) return -3;
	if (!m_moduleParams.isSyncName(sSyncName+",")) return 0;

#ifdef USES_PRINT_SYNC_DEBUG
	printf("***** sendSyncData(SyncName=%s,%s)...\n",sSyncName.c_str(),std::string(sDataString,nDataSize).c_str());
#endif

	if (m_pSyncErrorStoped || (m_pSyncSotpClientList.empty() && m_tProcSyncHostsTime>0))
	{
//#ifdef USES_PRINT_SYNC_DEBUG
//		printf("***** m_pSyncSotpClientList.empty() ERROR\n");
//#endif
		return -1;
	}
	if (!loadSyncData(true))
		return -1;

	const cgc::bigint nDataId = GetNextBigId(false);
	if (m_pSyncCdbcService.get()!=NULL)
	{
		const int nBufferSize = nDataSize+256;
		char * lpszBuffer = new char[nBufferSize];
		if (lpszBuffer==NULL) return -4;

		//tstring sSyncNameTemp(sSyncName);
		//m_pSyncCdbcService->escape_string_in(sSyncNameTemp);
		const int nBackup = bBackup?1:0;
		if (sDataString==NULL || nDataSize==0)
			sprintf(lpszBuffer,"INSERT INTO mycp_sync_data_t(data_id,type,name,backup) VALUES(%lld,%d,'%s',%d)",nDataId,nDataType,sSyncName.c_str(),nBackup);
		else
		{
			tstring sSyncDataTemp(sDataString,nDataSize);
			m_pSyncCdbcService->escape_string_in(sSyncDataTemp);
			sprintf(lpszBuffer,"INSERT INTO mycp_sync_data_t(data_id,type,name,data,backup) VALUES(%lld,%d,'%s','%s',%d)",nDataId,nDataType,sSyncName.c_str(),sSyncDataTemp.c_str(),nBackup);
		}
		const cgc::bigint ret = m_pSyncCdbcService->execute(lpszBuffer);
		if (ret!=1)
		{
			delete[] lpszBuffer;
			return -2;
		}

		CLockMap<tstring,mycp::CCGCSotpClient::pointer>::const_iterator pIter = m_pSyncSotpClientList.begin();
		for (; pIter!=m_pSyncSotpClientList.end(); pIter++)
		{
			const tstring sHost = pIter->first;
			sprintf(lpszBuffer,"INSERT INTO mycp_sync_request_t(request_id,data_id,host) VALUES(%lld,%lld,'%s')",GetNextBigId(true),nDataId,sHost.c_str());
			m_pSyncCdbcService->execute(lpszBuffer);
		}
		delete[] lpszBuffer;
	}
	// 
	if (sDataString==NULL || nDataSize==0)
		m_pSyncList.add(CSyncDataInfo::create(nDataId, nDataType, sSyncName, bBackup));
	else
		m_pSyncList.add(CSyncDataInfo::create(nDataId, nDataType, sSyncName, std::string(sDataString,nDataSize), bBackup));
	return 1;
}
int CModuleImpl::sendSyncFile(const tstring& sSyncName, int nDataType, const tstring& sFileName, const tstring& sFilePath, bool bBackup)
{
	return -1;
}

const DoSotpClientHandler::pointer NullDoSotpClientHandler;
DoSotpClientHandler::pointer CModuleImpl::getSotpClientHandler(const tstring& sAddress, const tstring& sAppName, SOTP_CLIENT_SOCKET_TYPE nSocketType, bool bUserSsl, bool bKeepAliveSession, int bindPort, int nThreadStackSize)
{
#ifndef USES_SOTP_CLIENT
	return NullDoSotpClientHandler;
#else
	//printf("**** getSotpClientHandler(%s)...\n",sAddress.c_str());
	if (sAddress.empty() || sAppName.empty()) return NullDoSotpClientHandler; 
	const tstring sKey = sAddress+sAppName;
	mycp::CCGCSotpClient::pointer pPOPSotpClient;
	if (!m_pModuleSotpClientList.find(sKey, pPOPSotpClient))
	{
		pPOPSotpClient = mycp::CCGCSotpClient::create();
		// ***（暂时不使用） 使用会报 do_event_loop std::exception. boost: mutex lock failed in pthread_mutex_lock: Invalid argument
		//pPOPSotpClient->SetIoService(getIoService(true),false);
		const CCgcAddress::SocketType nCgcSocketType = nSocketType==cgcApplication2::SOTP_CLIENT_SOCKET_TYPE_TCP?CCgcAddress::ST_TCP:CCgcAddress::ST_UDP;
		CCgcAddress pCgcAddress(sAddress,nCgcSocketType);
		if (!pPOPSotpClient->StartClient(pCgcAddress,bindPort,nThreadStackSize))
		{
			log(cgc::LOG_ERROR, "StartClient(%s) error.\n",sAddress.c_str());
			pPOPSotpClient.reset();
			return NullDoSotpClientHandler;
		}
		pPOPSotpClient->SetAppName(sAppName);
		//pPOPSotpClient->sotp()->doSetResponseHandler((CgcClientHandler*)this);
		pPOPSotpClient->sotp()->doSetConfig(SOTP_CLIENT_CONFIG_SOTP_VERSION,SOTP_PROTO_VERSION_21);
		pPOPSotpClient->sotp()->doSetConfig(SOTP_CLIENT_CONFIG_USES_SSL,(int)(bUserSsl?1:0));
		if (bKeepAliveSession)
		{
			pPOPSotpClient->sotp()->doSendOpenSession();
			if (nSocketType==cgcApplication2::SOTP_CLIENT_SOCKET_TYPE_TCP)
				pPOPSotpClient->sotp()->doStartActiveThread(30);
			else
				pPOPSotpClient->sotp()->doStartActiveThread(20);
			pPOPSotpClient->SetUserData(60);
		}else
		{
			pPOPSotpClient->SetUserData(30);
		}
		//pPOPSotpClient->sotp()->doSetEncoding(_T("UTF8"));
		//pPOPSotpClient->sotp()->doSetCIDTResends(max(1,m_moduleParams.getErrorRetry()),5);	// ? 5无效
		m_pModuleSotpClientList.insert(sKey,pPOPSotpClient);
	}
	//printf("**** getSotpClientHandler(%s) ok\n",sAddress.c_str());
	return pPOPSotpClient->sotp();
#endif
}

//void CModuleImpl::do_thread(CModuleImpl * pOwner)
//{
//	pOwner->procSyncThread();
//}
void CModuleImpl::OnCgcResponse(const cgcParserSotp & response)
{
	const unsigned long nSotpCallId = response.getCallid();
	const long nResultValue = response.getResultValue();
	//if (response.g
	ProcSyncResponse(nSotpCallId, nResultValue);

	//// -102: 错误模块代码
	//if (nResultValue < 0)
	////if (nResultValue == -102 || nResultValue == -103)
	//{
	//	return;
	//}
}

void CModuleImpl::OnCidTimeout(unsigned long callid, unsigned long sign, bool canResendAgain)
{
	if (!canResendAgain)
	{
		ProcSyncResponse(callid, -2);
	}
}

void CModuleImpl::ProcSyncResponse(unsigned long nCallId, int nResultValue)
{
	// nResultValue=2: Sync Update>0	(Database Update>0)
	// nResultValue=1: Sync Update=0	(Database Update=0)
	// nResultValue=0: Sync Warning		(Database Update=-1)
	// nResultValue=-2: OnCidTimeout
	// nResultValue<0: other error
	// ??? 需要考虑保存错误日志
	// 如果返回错误，是否继续同步下一条数据；
	// nResultValue>=0: 直接删除
	// ?其他情况，是否重试，如果重试 不能 remove m_pSyncRequestList 数据；

#ifdef USES_PRINT_SYNC_DEBUG
	printf("**** ProcSyncResponse call_id=%d,ResultValue=%d\n",(int)nCallId,nResultValue);
#endif
	const bool bSyncOk = nResultValue>=1?true:false;
	const bool bSyncWarning = nResultValue==0?true:false;
	// * 同步成功，或者设置错误继续，删除数据，保证同步线程能够处理下一个
	const bool bRemoveSyncData = (bSyncOk ||
		(bSyncWarning && m_moduleParams.getSyncResultProcessMode()!=CGC_SYNC_RESULT_PROCESS_WARNING_STOP) ||
		m_moduleParams.getSyncResultProcessMode()>=CGC_SYNC_RESULT_PROCESS_BACKUP_SYNC_NEXT)?true:false;

	CCGCSotpRequestInfo::pointer pRequestInfo;
	if (!m_pSyncRequestList.find(nCallId, pRequestInfo, bRemoveSyncData))
	{
		return;
	}
	if ((!bSyncOk && m_moduleParams.getSyncResultProcessMode()==CGC_SYNC_RESULT_PROCESS_ERROR_STOP) || 
		(bSyncWarning && m_moduleParams.getSyncResultProcessMode()==CGC_SYNC_RESULT_PROCESS_WARNING_STOP))
	{
		// stop
		log(cgc::LOG_ERROR, "CGC_SYNC_STOP\n");
		m_pSyncErrorStoped = true;
		m_pSyncList.clear();				// **
		m_pSyncRequestList.remove(nCallId);
		return;
	}
	if (m_pSyncCdbcService.get()==NULL) return;

	const tstring sHost(pRequestInfo->m_pRequestList.getParameterValue("host", ""));
	//printf("**** HOST : (%s)\n",sHost.c_str());
	char lpszBuffer[256];
	if (nResultValue>=0 ||	// ok or warning
		m_moduleParams.getSyncResultProcessMode()==CGC_SYNC_RESULT_PROCESS_DELETE_SYNC_NEXT)
	{
		sprintf(lpszBuffer,"DELETE FROM mycp_sync_request_t WHERE data_id=%lld AND host='%s'",pRequestInfo->m_pSyncDataInfo->m_nDataId,sHost.c_str());
		m_pSyncCdbcService->execute(lpszBuffer);

		sprintf(lpszBuffer,"SELECT data_id FROM mycp_sync_request_t WHERE data_id=%lld LIMIT 1",pRequestInfo->m_pSyncDataInfo->m_nDataId);
		if (m_pSyncCdbcService->select(lpszBuffer)==0)
		{
#ifdef USES_PRINT_SYNC_DEBUG
			printf("**** DELETE...\n");
#endif
			sprintf(lpszBuffer,"DELETE FROM mycp_sync_data_t WHERE data_id=%lld",pRequestInfo->m_pSyncDataInfo->m_nDataId);
			m_pSyncCdbcService->execute(lpszBuffer);
		}
	}else if (m_moduleParams.getSyncResultProcessMode()==CGC_SYNC_RESULT_PROCESS_BACKUP_SYNC_NEXT)	// * sync error
	{
		const int nFlag = nResultValue>=0?1:nResultValue;
#ifdef USES_PRINT_SYNC_DEBUG
		printf("**** CGC_SYNC_RESULT_PROCESS_BACKUP_SYNC_NEXT (flag=%d)\n",nFlag);
#endif
		sprintf(lpszBuffer,"UPDATE mycp_sync_data_t SET flag=%d WHERE data_id=%lld",nFlag,pRequestInfo->m_pSyncDataInfo->m_nDataId);
		m_pSyncCdbcService->execute(lpszBuffer);
	}
}

unsigned long CModuleImpl::getNextCallId(void)
{
	boost::mutex::scoped_lock lock(m_mutexCid);
	return (++m_nCurrentCallId)==0?1:m_nCurrentCallId;
}
bool CModuleImpl::FileIsExist(const char* lpszFile)
{
	FILE * f = fopen(lpszFile,"r");
	if (f==NULL)
		return false;
	fclose(f);
	return true;
}

bool CModuleImpl::loadSyncData(bool bCreateForce)
{
	if (!m_moduleParams.isSyncEnable() || m_moduleParams.getSyncHosts().empty()) return false;
	//const bool bFirstLoadSyncData = m_pSyncCdbcService.get()==NULL?true:false;
	if (m_pSyncCdbcService.get()==NULL)
	{
		const tstring sDatabaseName("mycp_sync_database");
#ifdef WIN32
		const tstring sDatabasePath = getAppConfPath() + "\\" + sDatabaseName;
#else
		const tstring sDatabasePath = getAppConfPath() + "/" + sDatabaseName;
#endif
		if (!bCreateForce && !FileIsExist(sDatabasePath.c_str())) return false;

		const std::string& sCdbcService = m_moduleParams.getSyncCdbcService();
		if (m_pServiceManager!=NULL && !sCdbcService.empty())
		{
			cgcServiceInterface::pointer serviceInterface = m_pServiceManager->getService(sCdbcService);
			if (serviceInterface.get() == NULL)
			{
				log(cgc::LOG_ERROR, "getService(%s) NULL ERROR\n", sCdbcService.c_str());
				return false;
			}

			m_pSyncCdbcService = CGC_CDBCSERVICE_DEF(serviceInterface);
			if (!m_pSyncCdbcService->initService())
			{
				m_pServiceManager->resetService(m_pSyncCdbcService);
				m_pSyncCdbcService.reset();
				log(cgc::LOG_ERROR, "%s Service initService() error\n", sCdbcService.c_str());
				return false;
			}

			cgcCDBCInfo::pointer m_cdbcInfo = cgcCDBCInfo::pointer(new cgcCDBCInfo(sDatabaseName,sDatabaseName));
			m_cdbcInfo->setConnection(sDatabasePath);
			if (!m_pSyncCdbcService->open(m_cdbcInfo))
			{
				m_pServiceManager->resetService(m_pSyncCdbcService);
				m_pSyncCdbcService.reset();
				log(cgc::LOG_ERROR, "%s Service open(%s) error\n", sCdbcService.c_str(),sDatabaseName.c_str());
				return false;
			}

			if (m_pSyncCdbcService->select("SELECT data_id FROM mycp_sync_data_t LIMIT 1")==-1)
			{
				const char* pCreateSyncDatabaseTable = "CREATE TABLE mycp_sync_data_t "\
					"( "\
					"data_id BIGINT DEFAULT 0 PRIMARY KEY, "\
					"type INT DEFAULT 0, "\
					"name VARCHAR(64) DEFAULT '', "\
					"data VARCHAR(4096) DEFAULT '', "\
					"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "\
					"backup SMALLINT DEFAULT 0, "\
					"flag SMALLINT DEFAULT 0 "\
					"); "\
					"CREATE TABLE mycp_sync_request_t "\
					"( "\
					"request_id BIGINT DEFAULT 0 PRIMARY KEY, "\
					"data_id BIGINT DEFAULT 0, "\
					"host VARCHAR(64) DEFAULT '', "\
					"flag SMALLINT DEFAULT 0 "\
					"); "\
					"CREATE INDEX mycp_sync_request_t_idx_data_id ON mycp_sync_request_t(data_id); ";
				if (m_pSyncCdbcService->execute(pCreateSyncDatabaseTable)==-1)
				{
					log(cgc::LOG_ERROR, "create %s Error.\n", sDatabaseName.c_str());
					m_pServiceManager->resetService(m_pSyncCdbcService);
					m_pSyncCdbcService.reset();
					return false;
				}
				log(cgc::LOG_INFO, "create %s Ok.\n", sDatabaseName.c_str());
			}
		}
	}
	StartSyncThread();
	return true;
}
void CModuleImpl::StartSyncThread(void)
{
	if (m_pSyncThread.get()==NULL)
	{
		m_pSyncThreadKilled = false;
		boost::thread_attributes attrs;
		attrs.set_stack_size(CGC_THREAD_STACK_MIN);
		m_pSyncThread = boost::shared_ptr<boost::thread>(new boost::thread(attrs,boost::bind(&CModuleImpl::procSyncThread, this)));
		log(cgc::LOG_INFO, "StartSyncThread ok\n");
	}
}

bool CModuleImpl::ProcSyncHosts(void)
{
	tstring sWhereInHostString;	// 'host1','host2'
	for (size_t i=0; i<m_pHostList.size(); i++)
	{
		const tstring sSyncHost = m_pHostList[i];
#ifdef USES_PRINT_SYNC_DEBUG
		printf("***** %d : SyncHost=%s\n",(int)i,sSyncHost.c_str());
#endif
		mycp::CCGCSotpClient::pointer m_pCGCSotpClient;
		if (!m_pSyncSotpClientList.find(sSyncHost,m_pCGCSotpClient) || !m_pCGCSotpClient->IsClientStarted())
		{
			if (m_pCGCSotpClient.get()!=NULL)
				m_pSyncSotpClientList.remove(sSyncHost);

			//CSotpClient m_pCgcClient;
			//m_pCgcClient.SetIoService(getIoService(true),false);
#ifdef USES_SOTP_CLIENT
			m_pCGCSotpClient = mycp::CCGCSotpClient::create();
			const CCgcAddress::SocketType nSocketType = m_moduleParams.getSyncSocketType()==1?CCgcAddress::ST_TCP:CCgcAddress::ST_UDP;
			CCgcAddress pCgcAddress(sSyncHost,nSocketType);
			if (!m_pCGCSotpClient->StartClient(pCgcAddress))
			{
				log(cgc::LOG_ERROR, "start cgc_sotopclient(%s) error.\n",sSyncHost.c_str());
				continue;
			}
			const std::string sAppName = getApplicationName();
			m_pCGCSotpClient->SetAppName(sAppName);
			m_pCGCSotpClient->sotp()->doSetResponseHandler((CgcClientHandler*)this);
			m_pCGCSotpClient->sotp()->doSetConfig(SOTP_CLIENT_CONFIG_SOTP_VERSION,SOTP_PROTO_VERSION_21);
			m_pCGCSotpClient->sotp()->doSetConfig(SOTP_CLIENT_CONFIG_USES_SSL,1);
			m_pCGCSotpClient->sotp()->doSendOpenSession();
			m_pCGCSotpClient->sotp()->doSetEncoding(_T("UTF8"));
			m_pCGCSotpClient->sotp()->doStartActiveThread(25);
			m_pCGCSotpClient->sotp()->doSetCIDTResends(max(1,m_moduleParams.getErrorRetry()),5);	// ? 5无效
			m_pSyncSotpClientList.insert(sSyncHost,m_pCGCSotpClient,true);
#endif
		}
		if (!sWhereInHostString.empty())
		{
			sWhereInHostString.append(",");
		}
		sWhereInHostString.append("'");
		sWhereInHostString.append(sSyncHost);
		sWhereInHostString.append("'");
	}
	if (m_pSyncSotpClientList.empty())
		return false;

	if (m_pSyncCdbcService.get()!=NULL && !m_bLoadBackupFromCdbcService)
	{
		m_bLoadBackupFromCdbcService = true;
		// * 删除无用同步 host 请求，mycp_sync_request_t，使用最新配置 sync_hosts
		char lpszBuffer[1024]; //  
		sprintf(lpszBuffer,"DELETE FROM mycp_sync_request_t WHERE host NOT IN (%s)",sWhereInHostString.c_str());
		m_pSyncCdbcService->execute(lpszBuffer);

		// ? 是否优化，一次不要加载太多，避免同步服务器停机，数据太多；
		int cdbcCookie = 0;
		m_pSyncCdbcService->select("SELECT data_id,type,name,data,backup FROM mycp_sync_data_t WHERE flag=0 ORDER BY create_time", cdbcCookie);
		cgcValueInfo::pointer record = m_pSyncCdbcService->first(cdbcCookie);
		while (record.get() != NULL)
		{
			const cgc::bigint nDataId = record->getVector()[0]->getBigIntValue();
			const int nDataType = record->getVector()[1]->getIntValue();
			const tstring sSyncName(record->getVector()[2]->getStr());
			tstring sSyncData(record->getVector()[3]->getStr());
			const bool bBackup = record->getVector()[4]->getIntValue()==1?true:false;
			record = m_pSyncCdbcService->next(cdbcCookie);

			// **不需要判断，预防 host 重复同步
			//// * 判断 mycp_sync_request_t 是否存在，不存在插入新请求数据
			//CLockMap<tstring,mycp::CCGCSotpClient::pointer>::const_iterator pIter = m_pSyncSotpClientList.begin();
			//for (; pIter!=m_pSyncSotpClientList.end(); pIter++)
			//{
			//	const tstring sHost = pIter->first;
			//	sprintf(lpszBuffer,"SELECT data_id FROM mycp_sync_request_t WHERE data_id=%lld AND host='%s' LIMIT 1",nDataId,sHost.c_str());
			//	if (m_pSyncCdbcService->select(lpszBuffer)==0)
			//	{
			//		sprintf(lpszBuffer,"INSERT INTO mycp_sync_request_t(request_id,data_id,host) VALUES(%lld,%lld,'%s')",GetNextBigId(true),nDataId,sHost.c_str());
			//		m_pSyncCdbcService->execute(lpszBuffer);
			//	}
			//}

			// * 添加到请求列表
			if (sSyncData.empty())
			{
				CSyncDataInfo::pointer pSyncInfo = CSyncDataInfo::create(nDataId, nDataType, sSyncName, bBackup);
				pSyncInfo->m_bFromDatabase = true;
				m_pSyncList.add(pSyncInfo);
			}else
			{
				m_pSyncCdbcService->escape_string_out(sSyncData);
				CSyncDataInfo::pointer pSyncInfo = CSyncDataInfo::create(nDataId, nDataType, sSyncName, sSyncData, bBackup);
				pSyncInfo->m_bFromDatabase = true;
				m_pSyncList.add(pSyncInfo);
			}
		}
		m_pSyncCdbcService->reset(cdbcCookie);
	}
	return true;
}
void CModuleImpl::procSyncThread(void)
{
	try
	{
		char lpszBuffer[512];
		//if (m_moduleParams.isSyncEnable())
		{
			const std::string& sSyncHosts = m_moduleParams.getSyncHosts();
			m_pHostList.clear();
			ParseString(sSyncHosts.c_str(),",",m_pHostList);
			ProcSyncHosts();
			m_tProcSyncHostsTime = time(0);
		}

		const bool bSyncSerialize = m_moduleParams.isSyncSerialize();
		while (!m_pSyncThreadKilled)
		{
			if (m_pSyncErrorStoped)
			{
				// ?
			}
			if (bSyncSerialize && !m_pSyncRequestList.empty())
			{
				// * 等待返回才处理下一个；
#ifdef WIN32
				Sleep(50);
#else
				usleep(50000);
#endif
				continue;
			}
			if ((m_pHostList.size()!=m_pSyncSotpClientList.size() || m_pSyncSotpClientList.empty()) &&
				(m_tProcSyncHostsTime+m_moduleParams.getConnectRetry())<=time(0))
			{
				// *连接数量不对 或者 没有host列表，定期检查一次；
				ProcSyncHosts();
				m_tProcSyncHostsTime = time(0);
			}
			CSyncDataInfo::pointer pSyncDataInfo;
			if (m_pSyncList.front(pSyncDataInfo))
			{
				// send
				bool bSendSyncData = false;
				CLockMap<tstring,mycp::CCGCSotpClient::pointer>::const_iterator pIter = m_pSyncSotpClientList.begin();
				for (; pIter!=m_pSyncSotpClientList.end(); pIter++)
				{
					const tstring sHost = pIter->first;
#ifdef USES_PRINT_SYNC_DEBUG
					printf("***** send to (%s)\n",sHost.c_str());
#endif
					if (pSyncDataInfo->m_bFromDatabase && m_pSyncCdbcService.get()!=NULL)
					{
						// * 从数据库恢复数据，需要判断数据库 host 是否不存在，如果不存在，说明已经同步过
						sprintf(lpszBuffer,"SELECT data_id FROM mycp_sync_request_t WHERE data_id=%lld AND host='%s' LIMIT 1",pSyncDataInfo->m_nDataId,sHost.c_str());
						if (m_pSyncCdbcService->select(lpszBuffer)!=1)
						{
							// * 不存在，说明已经同步过
							continue;
						}
					}

					mycp::CCGCSotpClient::pointer m_pCGCSotpClient = pIter->second;
					const bool bDatasource = m_pDataSourceList.exist(pSyncDataInfo->m_sName);
					m_pCGCSotpClient->sotp()->doBeginCallLock();
					int nExtData = 0;
					if (bDatasource)
						nExtData |= 1;

					if (nExtData!=0)
						m_pCGCSotpClient->sotp()->doAddParameter(CGC_PARAMETER("ext", nExtData));
					if (pSyncDataInfo->m_nType!=0)
						m_pCGCSotpClient->sotp()->doAddParameter(CGC_PARAMETER("type", pSyncDataInfo->m_nType));
					m_pCGCSotpClient->sotp()->doAddParameter(CGC_PARAMETER("data", pSyncDataInfo->m_sData),false);

					const unsigned long nCallId = getNextCallId();
					CCGCSotpRequestInfo::pointer pRequestInfo = CCGCSotpRequestInfo::create(nCallId);
					pRequestInfo->m_pRequestList.SetParameter(CGC_PARAMETER("host", sHost));
					pRequestInfo->m_pSyncDataInfo = pSyncDataInfo;
					m_pSyncRequestList.insert(nCallId, pRequestInfo);
					m_pCGCSotpClient->sotp()->doSendSyncCall(nCallId, pSyncDataInfo->m_sName, true);
					bSendSyncData = true;
				}
				if (!bSendSyncData && m_pSyncCdbcService.get()!=NULL && !m_pSyncSotpClientList.empty())
				{
					// * 没有发送，判断是否无用同步数据（已经全部同步过）
					sprintf(lpszBuffer,"SELECT data_id FROM mycp_sync_request_t WHERE data_id=%lld LIMIT 1",pSyncDataInfo->m_nDataId);
					if (m_pSyncCdbcService->select(lpszBuffer)==0)
					{
						// * 无用同步数据，直接删除
						sprintf(lpszBuffer,"DELETE FROM mycp_sync_data_t WHERE data_id=%lld",pSyncDataInfo->m_nDataId);
						m_pSyncCdbcService->execute(lpszBuffer);
					}
				}
				continue;
			}

#ifdef WIN32
			Sleep(50);
#else
			usleep(50000);
#endif
		}
	}catch (const std::exception&)
	{
	}catch (...)
	{
	}
}

void CModuleImpl::log(LogLevel level, const char* format,...)
{
	if (m_logService.get() != NULL)
	{
		if (!m_logService->isLogLevel(level))
			return;
	}else if (!m_moduleParams.isLts())
	//}else if ((m_moduleParams.getLogLevel()&(int)level)==0)
	{
		return;
	}
	try
	{
		char debugmsg[MAX_LOG_SIZE];
		//memset(debugmsg, 0, MAX_LOG_SIZE);
		va_list   vl;
		va_start(vl, format);
		int len = vsnprintf(debugmsg, MAX_LOG_SIZE-1, format, vl);
		va_end(vl);
		if (len > MAX_LOG_SIZE)
			len = MAX_LOG_SIZE;
		else if (len<=0)
			return;
		debugmsg[len] = '\0';

		if (m_logService.get() != NULL)
		{
			m_logService->log2(level, debugmsg);
		}else if (m_moduleParams.isLts())
		{
			std::cerr << debugmsg;
		}
	}catch(std::exception const &)
	{
		return;
	}catch(...)
	{
		return;
	}

}

void CModuleImpl::log(LogLevel level, const wchar_t* format,...)
{
	if (m_logService.get() != NULL)
	{
		if (!m_logService->isLogLevel(level))
			return;
	}else if (!m_moduleParams.isLts())
	//}else if ((m_moduleParams.getLogLevel()&(int)level)==0)
	{
		return;
	}
	try
	{
		//boost::mutex::scoped_lock lock(m_mutex2);
		wchar_t debugmsg[MAX_LOG_SIZE];
		//memset(debugmsg, 0, MAX_LOG_SIZE);
		va_list   vl;
		va_start(vl, format);
		int len = vswprintf(debugmsg, MAX_LOG_SIZE-1, format, vl);
		va_end(vl);
		if (len > MAX_LOG_SIZE)
			len = MAX_LOG_SIZE;
		debugmsg[len] = L'\0';

		if (m_logService.get() != NULL)
		{
			m_logService->log2(level, debugmsg);
		}else if (m_moduleParams.isLts())
		{
			std::wcerr<< debugmsg;
		}
	}catch(std::exception const &)
	{
		return;
	}catch(...)
	{
		return;
	}
}


// CModuleMgr
CModuleMgr::CModuleMgr(void)
{
}

CModuleMgr::~CModuleMgr(void)
{
	FreeHandle();
}

void CModuleMgr::OnCheckTimeout(void)
{
	BoostReadLock rdlock(m_mapModuleImpl.mutex());
	CLockMap<void*, cgcApplication::pointer>::iterator pIter;
	for (pIter=m_mapModuleImpl.begin(); pIter!=m_mapModuleImpl.end(); pIter++)
	{
		CModuleImpl * pModuleImpl = (CModuleImpl*)pIter->second.get();
		pModuleImpl->OnCheckTimeout();
	}
}

void CModuleMgr::StopModule(void)
{
	BoostReadLock rdlock(m_mapModuleImpl.mutex());
	//for_each(m_mapModuleImpl.begin(), m_mapModuleImpl.end(),
	//	boost::bind(&CModuleMgr::stopModule, boost::bind(&std::map<unsigned long, cgcApplication::pointer>::value_type::second,_1)));

	CLockMap<void*, cgcApplication::pointer>::iterator pIter;
	for (pIter=m_mapModuleImpl.begin(); pIter!=m_mapModuleImpl.end(); pIter++)
	{
		CModuleImpl * pModuleImpl = (CModuleImpl*)pIter->second.get();
		pModuleImpl->StopModule();
	}
}

void CModuleMgr::FreeHandle(void)
{
	StopModule();
	m_mapModuleImpl.clear();
}


