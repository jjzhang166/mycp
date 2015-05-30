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

//#define USES_HDCID 1
#ifdef WIN32
#include <Windows.h>
#else //WIN32
#include "dlfcn.h"
unsigned long GetLastError(void)
{
	return 0;
}
#endif

#include "CGCApp.h"
#include "md5.h"

#ifdef WIN32
#include <tchar.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include "../../../CGCClass/tchar.h"
#endif // WIN32

//#ifdef USES_HDCID
//#include "HDComputerID/def.h"
//#ifdef WIN32
//#ifdef _DEBUG
//#pragma comment(lib, L"HDComputerIDd.lib")
//#else
//#pragma comment(lib, L"HDComputerID.lib")
//#endif
//#endif
//
//tstring getAccountKey(int count, const tstring & sAccount, long & hdcid)
//{
//	hdcid = getHardDriveComputerID();
//
//	//
//	// to md5 & return
//	//
//	tstring sKeySrc = cgcString::Format("%d%s%d", count, sAccount.c_str(), hdcid);
//	tstring s = cgcString::toMd5(sKeySrc.c_str(), sKeySrc.length());
//	return  cgcString::toUpper(s);
//}
//#endif

CGCApp::CGCApp(const tstring & sPath)
: m_bStopedApp(true)
, m_bInitedApp(false)
, m_bExitLog(false)
, m_sModulePath(sPath)
, m_pProcSessionTimeout(NULL)
, m_pProcDataResend(NULL)
, m_bLicensed(true)
, m_sLicenseAccount(_T(""))
, m_licenseModuleCount(1)
, m_fpGetLogService(NULL), m_fpResetLogService(NULL), m_fpParserSotpService(NULL), m_fpParserHttpService(NULL)
, /*m_fpHttpStruct(NULL), */m_fpHttpServer(NULL), m_sHttpServerName("")
//, m_pRtpSession(true)
, m_tLastNewParserSotpTime(0), m_tLastNewParserHttpTime(0)

{
//#ifdef WIN32
//	TCHAR chModulePath[MAX_PATH];
//	memset(&chModulePath, 0, MAX_PATH);
//	::GetModuleFileName(NULL, chModulePath, MAX_PATH);
//	TCHAR* temp = (TCHAR*)0;
//	temp = _tcsrchr(chModulePath, (unsigned int)'\\');
//	chModulePath[temp - chModulePath] = '\0';
//
//	m_sModulePath = chModulePath;
//#else
//	namespace fs = boost::filesystem;
//	fs::path currentPath( fs::initial_path());
//	m_sModulePath = currentPath.string();
//#endif
	//m_pRtpSession.SetSotpRtpCallback((CSotpRtpCallback*)this);
	m_logModuleImpl.setModulePath(m_sModulePath);
}

CGCApp::~CGCApp(void)
{
	AppExit();
}

void CGCApp::PrintHelp(void)
{
	std::cout << "\n********************* App Help *********************\n";
	std::cout << "\thelp\t\tPrint this help.\n";
	std::cout << "\tstart\t\tStart MYCP Service.\n";
	std::cout << "\tstop\t\tStop MYCP Service.\n";
	std::cout << "\trestart\t\tRestart MYCP Service.\n";
	std::cout << "\texit\t\tExit MYCP Server.\n";

}

/* ?
void do_event_loop(void)
{
	ACE_Reactor::instance()->run_reactor_event_loop();
}
*/

void do_dataresend(CGCApp * pCGCApp)
{
	if (pCGCApp == NULL) return;

	while (pCGCApp->isInited())
	{
		try
		{
			if (!pCGCApp->ProcDataResend())
			{
#ifdef WIN32
				Sleep(500);
#else
				usleep(500000);
#endif
			}
		}catch(std::exception const &)
		{
		}catch(...){
		}
	}
}
void do_sessiontimeout(CGCApp * pCGCApp)
{
	if (pCGCApp == NULL) return;

	// 1 秒检查一次，SESSION是否超时没有访问。自动清除无用SESSION
	unsigned int theIndex = 0;
	while (pCGCApp->isInited())
	{
#ifdef WIN32
		Sleep(100);
#else
		usleep(100000);
#endif
		if (((theIndex++)%10)!=9)
			continue;
		try
		{

			pCGCApp->ProcCheckParserPool();
			pCGCApp->ProcNotKeepAliveRmote();
			if ((++theIndex%200)!=199) continue;	// 100*200=20*1000=20秒处理一次

			// 如果没有超时SESSION，再继续等待
			while(pCGCApp->ProcLastAccessedTime())
			{
				// 如果有超时SESSION，继续处理；
#ifdef WIN32
				Sleep(1000);
#else
				sleep(1);
#endif
				pCGCApp->ProcNotKeepAliveRmote();
			}
		}catch(std::exception const &)
		{
		}catch(...){
		}
	}
}

void CGCApp::ProcNotKeepAliveRmote(void)
{
	// 检查没有keep-alive连接；
	const time_t tNow = time(0);
	CNotKeepAliveRemote::pointer pNotKeepAliveRemote;
	while (m_pNotKeepAliveRemoteList.front(pNotKeepAliveRemote))
	{
		if (!pNotKeepAliveRemote->IsExpireTime(tNow, 2))
		{
			m_pNotKeepAliveRemoteList.pushfront(pNotKeepAliveRemote); 
			break;
		}
	}
}
bool CGCApp::ProcLastAccessedTime(void)
{
	std::string sCloseSid;
	this->m_mgrSession.ProcLastAccessedTime(sCloseSid);
	if (!sCloseSid.empty())
	{
		m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\' closed\n"), sCloseSid.c_str());
		return true;
	}

	static unsigned int nIndex = 0;
	if ((nIndex++)%3==2)	// 3*20=60秒处理一次；
	{
		BoostReadLock rdlock(m_pRtpSession.mutex());
		CLockMap<int,CSotpRtpSession::pointer>::iterator pIter = m_pRtpSession.begin();
		for (; pIter!=m_pRtpSession.end(); pIter++)
		{
			CSotpRtpSession::pointer pRtpSession = pIter->second;
			pRtpSession->CheckRegisterSourceLive(59, this, 0);
		}
		//m_pRtpSession.CheckRegisterSourceLive(59, this, 0);
	}
	return false;
	//// 检查mysessioninfo
	//{
	//	boost::mutex::scoped_lock lock(m_pMySessionInfoList.mutex());
	//	CLockMap<tstring,CMySessionInfo::pointer>::iterator pIter = m_pMySessionInfoList.begin();
	//	for (; pIter!=m_pMySessionInfoList.end(); pIter++)
	//	{
	//		CMySessionInfo::pointer pMySessionInfo = pIter->second;
	//		if (pMySessionInfo->GetRequestTime()+10*60 < time(0))
	//		{
	//			// mysessionif过期
	//			m_pMySessionInfoList.erase(pIter);
	//			break;	// 2秒检查一个，时间应该足够，不够以后再优化
	//		}
	//	}
	//}
}

#ifdef _UNICODE
typedef boost::filesystem::wpath boosttpath;
#else
typedef boost::filesystem::path boosttpath;
#endif // _UNICODE
void CGCApp::AppInit(bool bNTService)
{
	// init parameter
	tstring xmlFile(m_sModulePath);
	xmlFile.append(_T("/conf/params.xml"));
	namespace fs = boost::filesystem;
	boosttpath pathXmlFile(xmlFile);
	if (fs::exists(pathXmlFile))
	{
		m_logModuleImpl.m_moduleParams.load(xmlFile);
	}
	if (m_logModuleImpl.m_moduleParams.getLogFile().empty())
		m_logModuleImpl.m_moduleParams.setLogFile(_T("MYCP.log"));
	//m_logModuleImpl.m_moduleParams.setLts(false);
	
	if (bNTService)
	{
		// ????
		/*
		tstring sAceLogConf(m_sModulePath);
		sAceLogConf.append(L"/cwss_acelog.conf");
		if (ACE_LOG_MSG->open(sAceLogConf.c_str(), ACE_Log_Msg::LOGGER, ACE_DEFAULT_LOGGER_KEY) < 0)
		{
			//		ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT("%p\n"), ACE_TEXT("Service Config open")), 1);
			m_logModuleImpl.log(DL_WARNING, L"Service Config open error, LastError = 0x%x!\n", GetLastError());
		}*/
	}
//#ifndef WIN32
//	pthread_attr_t    attr;
//	size_t            size;
//	/* do some calculation for optimal stack size or ... */
//	size = PTHREAD_STACK_MIN;
//	printf("**** PTHREAD_STACK_MIN=%d\n",size);
//	pthread_attr_init(&attr);
//	pthread_attr_setstacksize(&attr, size);
//#endif
	// 必须放在前面
	m_bInitedApp = true;
	m_bExitLog = false;
	boost::thread_attributes attrs;
	attrs.set_stack_size(CGC_THREAD_STACK_MIN);
	m_pProcSessionTimeout = new boost::thread(attrs,boost::bind(&do_sessiontimeout, this));
	m_pProcDataResend = new boost::thread(attrs,boost::bind(&do_dataresend, this));
}

void CGCApp::AppStart(void)
{
	if (!m_bStopedApp)
	{
		return;
	}
	m_pSotpParserPool.clear();
	m_pHttpParserPool.clear();
	m_bStopedApp = false;

	LoadDefaultConf();
	m_logModuleImpl.log(LOG_INFO, _T("Starting %s Service......\n"), m_parseDefault.getCgcpName().c_str());

	// optional conf
	LoadClustersConf();
//	LoadAuthsConf();
	LoadSystemParams();

	for (int i=0; i<m_parseDefault.getWaitSleep(); i++)
	{
#ifdef WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
		if (m_bStopedApp)
			break;
	}

	LoadModulesConf();
}

void CGCApp::AppStop(void)
{
	if (m_bStopedApp)
	{
		return;
	}
	m_logModuleImpl.log(LOG_INFO, _T("Stop %s Service......\n"), m_parseDefault.getCgcpName().c_str());
	m_bStopedApp = true;
	m_parsePortApps.clear();
	m_pSotpParserPool.clear();
	m_pHttpParserPool.clear();
	m_pRtpSession.clear();
	m_mgrSession.invalidates(true);
	FreeLibModules(MODULE_COMM);
	FreeLibModules(MODULE_APP);		// ***先停止APP应用
	FreeLibModules(MODULE_SERVER);
	FreeLibModules(MODULE_PARSER);
	m_mgrSession.FreeHandle();
	//m_mgrHttpSession.FreeHandle();

	if (m_logModuleImpl.logService().get() != NULL)
	{
		if (m_fpResetLogService != NULL)
			m_fpResetLogService(m_logModuleImpl.logService());
		m_logModuleImpl.logService(cgcNullLogService);
	}
	m_logModuleImpl.StopModule();
	FreeLibModules(MODULE_LOG);
	m_logModuleImpl.log(LOG_INFO, _T("**** %s Service stop succeeded ****\n"), m_parseDefault.getCgcpName().c_str());

	m_cdbcServices.clear();
	FreeLibrarys();
	m_pNotKeepAliveRemoteList.clear();
	m_parseModules.FreeHandle();
//	m_parseClusters.FreeHandle();
//	m_parseAuths.m_mapAuths.clear();
	m_systemParams.clearParameters();
	m_cdbcs.FreeHandle();
	m_parseWeb.FreeHandle();
	m_fpGetLogService = NULL;
	m_fpResetLogService = NULL;
	m_fpParserSotpService = NULL;
	m_fpParserHttpService = NULL;
	//m_fpHttpStruct = NULL;
	m_fpHttpServer = NULL;
	m_sHttpServerName = "";
	m_mapRemoteOpenSes.clear();
	//m_mapServiceModule.clear();
	m_mapMultiParts.clear();	// ??
	m_attributes.reset();
	//clearAllAtrributes();
	//cleanAllPropertys();
}

void CGCApp::AppExit(void)
{
	if (!m_bInitedApp) return;
	m_bInitedApp = false;

	m_logModuleImpl.log(LOG_INFO, _T("Exiting %s Server......\n"), m_parseDefault.getCgcpName().c_str());

	if (m_pProcSessionTimeout)
	{
		m_pProcSessionTimeout->join();
		delete m_pProcSessionTimeout;
		m_pProcSessionTimeout = NULL;
	}
	if (m_pProcDataResend)
	{
		m_pProcDataResend->join();
		delete m_pProcDataResend;
		m_pProcDataResend = NULL;
	}

	AppStop();
}

#ifndef WIN32
bool theKilledApp = false;
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
void kill_op(int signum,siginfo_t *info,void *myact)
{
	printf("receive signal %d\n", signum);
	//sleep(5);
	theKilledApp = true;
}
//void pipeSignalProc(int sig)
//{
//	switch(sig)
//	{
//	case SIGPIPE:
//		//p_log(LOG_DEBUG, "SIGPIPE");
//		break;
//	default:
//		//p_log(LOG_DEBUG, "Unknow signal!");
//		break;
//	}
//}
#endif

int CGCApp::MyMain(bool bServcie)
{
	AppInit(false);
	AppStart();

	//for (int i=0;i<20;i++)
	//{
	//	m_pRsa.SetPrivatePwd(GetSaltString());
	//	m_pRsa.rsa_generatekey_mem(1024);
	//	if (m_pRsa.GetPublicKey().empty() || m_pRsa.GetPrivateKey().empty())
	//	{
	//		m_pRsa.SetPrivatePwd("");
	//		continue;
	//	}
	//	if (!m_pRsa.rsa_open_private_mem())
	//	{
	//		m_pRsa.SetPrivatePwd("");
	//		continue;
	//	}
	//	const std::string sFrom("mycp");
	//	unsigned char* pTo = NULL;
	//	m_pRsa.rsa_private_encrypt((const unsigned char*)sFrom.c_str(),sFrom.size(),&pTo);
	//	if (pTo!=NULL)
	//	{
	//		delete[] pTo;
	//		m_pRsa.rsa_close_private();
	//		break;
	//	}
	//	m_pRsa.rsa_close_private();
	//	m_pRsa.SetPrivatePwd("");
	//}
	//if (m_pRsa.GetPrivatePwd().empty())
	//	std::cout << "[ERROR]Generate SSL Key error.\n";
	if (bServcie)
	{
		std::cout << "\n********************* App Help *********************\n";
		std::cout << "App Service Running...\n";
	}else
	{
		PrintHelp();
	}
#ifndef WIN32
		struct sigaction act;	
		sigemptyset(&act.sa_mask);
		act.sa_flags=SA_SIGINFO;
		act.sa_sigaction=kill_op;
		if(sigaction(SIGTERM,&act,NULL) < 0)
		{
			printf("install SIGTERM sigal error/n");
		}
		// 11) SIGSEGV 试图访问未分配给自己的内存, 或试图往没有写权限的内存地址写数据. 
		//if(sigaction(SIGSEGV ,&act,NULL) < 0)
		//{
		//	printf("install SIGSEGV sigal error/n");
		//}
		//signal(SIGPIPE, pipeSignalProc);
		// 忽略broken pipe信號
		// 11) SIGPIPE
		signal(SIGPIPE, SIG_IGN);
#endif
	//while (!m_bExitedApp)
#ifdef WIN32
	while (m_bInitedApp)
#else
	while (m_bInitedApp && !theKilledApp)
#endif
	{
		if (bServcie)
		{
#ifdef WIN32
			Sleep(10);
#else
			usleep(10000);
#endif
			continue;
		}
		std::wcout << _T("CMD:");
		tstring command;
		try
		{
			std::getline(std::cin, command);
			if (command.length() == 0)
			{
#ifdef WIN32
				//std::wcin.clear(std::ios_base::goodbit, true);
				std::wcin.clear(std::ios_base::goodbit);
#else
				//std::wcin.clear(std::ios_base::goodbit, true);
#endif
				continue;
			}
		}catch (const std::exception & e)
		{
			std::cout << e.what() << std::endl;
			continue;
		}catch (...)
		{
			std::cout << "some exception" << std::endl;
			continue;
		}

		//if (m_bExitedApp ||
		if (!m_bInitedApp ||
			command.compare(_T("exit")) == 0 ||
			command.compare(_T("x")) == 0)
		{
			break;
		}else if (command.compare(_T("help")) == 0 ||
			command.compare(_T("?")) == 0 ||
			command.compare(_T("h")) == 0)
		{
			PrintHelp();
		}else if (command.compare(_T("start")) == 0)
		{
			AppStart();
		}else if (command.compare(_T("stop")) == 0)
		{
			AppStop();
		}else if (command.compare(_T("restart")) == 0)
		{
			AppStop();
			AppStart();
		}else
		{
			std::cout << "ERROR CMD: ";
			std::cout << command.c_str() << "\n'h' or '?' for help!" << std::endl;
		}
	}

	AppExit();

	return 0;
}

void CGCApp::LoadDefaultConf(void)
{
	m_parseDefault.load(m_sModulePath + _T("/conf/default.xml"));

	m_logModuleImpl.log(LOG_DEBUG, _T("Server Name = '%s'\n"), m_parseDefault.getCgcpName().c_str());
	//m_logModuleImpl.log(LOG_DEBUG, _T("Server Address = '%s'\n"), m_parseDefault.getCgcpAddr().c_str());

	m_parsePortApps.load(m_sModulePath + _T("/conf/port-apps.xml"));

#ifdef USES_HDCID
	tstring iniLicenseFile(m_sModulePath);
	iniLicenseFile.append(L"/conf/license.ini");

	tstring sCode;

	//
	// read account & code from license.ini
	//
	char lpBuf[60];
	memset(lpBuf, 0, sizeof(lpBuf));
	::GetPrivateProfileStringA("license", L"account", L"", lpBuf, sizeof(lpBuf), iniLicenseFile.c_str());
	m_sLicenseAccount = lpBuf;
	::GetPrivateProfileStringA("license", L"code", L"", lpBuf, sizeof(lpBuf), iniLicenseFile.c_str());
	sCode = lpBuf;

	m_licenseModuleCount = ::GetPrivateProfileIntA("license", L"count", 1, iniLicenseFile.c_str());

	long hdcid = 0;
	tstring sAccountKey = getAccountKey(m_licenseModuleCount, m_sLicenseAccount, hdcid);
	if (sCode.empty() || sAccountKey.compare(sCode) != 0)
	{
		m_bLicensed = false;
		m_licenseModuleCount = 1;

		//
		// write to license.ini
		//
		tstring sHdcid = cgcString::Format("%d", hdcid);
		::WritePrivateProfileStringA("license", L"hdcid", sHdcid.c_str(), iniLicenseFile.c_str());
	}else
	{
		m_bLicensed = true;
	}

#endif
}

void CGCApp::LoadClustersConf(void)
{
	tstring xmlFile(m_sModulePath);
	xmlFile.append(_T("/conf/clusters.xml"));

	// ? SOTP/2.0
	/*
	int retCode = theXmlParse.ParseXmlFile(xmlFile.c_str(), &this->m_parseClusters);
	if (retCode == 0)
	{
		m_logModuleImpl.log(LOG_DEBUG, _T("Cluster Servers '%d'\n"), m_parseClusters.getClusters().size());
	}

	return retCode;
	*/
}

/*
void CGCApp::LoadAuthsConf(void)
{
	tstring xmlFile(m_sModulePath);
	xmlFile.append(_T("/conf/auths.xml"));

	namespace fs = boost::filesystem;
	boosttpath pathXmlFile(xmlFile);
	if (boost::filesystem::exists(pathXmlFile))
	{
		m_parseAuths.load(xmlFile);
	}
}
*/

void CGCApp::LoadModulesConf(void)
{
	int initModuleCount = 0;
	tstring xmlFile(m_sModulePath);
	xmlFile.append(_T("/conf/modules.xml"));

	m_parseModules.load(xmlFile);

	m_logModuleImpl.log(LOG_DEBUG, _T("MODULES = %d\n"), m_parseModules.m_modules.size());

	OpenLibrarys();
	InitLibModules(MODULE_LOG);
	InitLibModules(MODULE_SERVER|MODULE_PARSER|MODULE_APP);
	InitLibModules(MODULE_COMM);

	if (m_fpParserSotpService == NULL)
	{
		m_logModuleImpl.log(LOG_WARNING, _T("ParserService == NULL.\n"));
	}else
	{
		m_logModuleImpl.log(LOG_INFO, _T("**** %s Server start succeeded ****\n"), m_parseDefault.getCgcpName().c_str());
	}
}

void CGCApp::LoadSystemParams(void)
{
	// init parameter
	tstring xmlFile(m_sModulePath);
	xmlFile.append(_T("/conf/params.xml"));
	m_systemParams.load(xmlFile);
	m_logModuleImpl.log(LOG_DEBUG, _T("SystemParams = %d\n"), m_systemParams.getParameters()->size());

	xmlFile = m_sModulePath;
	xmlFile.append(_T("/conf/cdbcs.xml"));
	namespace fs = boost::filesystem;
	boosttpath pathXmlFile(xmlFile);
	if (boost::filesystem::exists(pathXmlFile))
	{
		m_cdbcs.load(xmlFile);
		m_logModuleImpl.log(LOG_DEBUG, _T("CDBCInfos = %d\n"), m_cdbcs.m_cdbcInfos.size());
		m_logModuleImpl.log(LOG_DEBUG, _T("DataSources = %d\n"), m_cdbcs.m_dsInfos.size());
	}

	xmlFile = m_sModulePath;
	xmlFile.append(_T("/conf/web.xml"));
	pathXmlFile = boosttpath(xmlFile);
	if (boost::filesystem::exists(pathXmlFile))
	{
		m_parseWeb.load(xmlFile);
		m_logModuleImpl.log(LOG_DEBUG, _T("Servlets = %d\n"), m_parseWeb.m_servlets.size());
	}
}

bool CGCApp::onRegisterSource(cgc::bigint nRoomId, cgc::bigint nSourceId, cgc::bigint nParam, void* pUserData)
{
	const int nServerPort = (int)pUserData;
	CPortApp::pointer portApp = m_parsePortApps.getPortApp(nServerPort);
	if (portApp.get() == NULL)
	{
		return true;	// *
	}
	if (portApp->getModuleHandle() == NULL)
	{
		ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(portApp->getApp());
		if (moduleItem.get() == NULL || moduleItem->getModuleHandle() == NULL)
		{
			return false;
		}
		void * hModule = moduleItem->getModuleHandle();
		portApp->setModuleHandle(hModule);

#ifdef WIN32
		FPCGC_Rtp_Register_Source fp1 = (FPCGC_Rtp_Register_Source)GetProcAddress((HMODULE)hModule, "CGC_Rtp_Register_Source");
		CGC_Rtp_UnRegister_Source fp2 = (CGC_Rtp_UnRegister_Source)GetProcAddress((HMODULE)hModule, "CGC_Rtp_UnRegister_Source");
		FPCGC_Rtp_Register_Sink fp3 = (FPCGC_Rtp_Register_Sink)GetProcAddress((HMODULE)hModule, "CGC_Rtp_Register_Sink");
#else
		FPCGC_Rtp_Register_Source fp1 = (FPCGC_Rtp_Register_Source)dlsym(hModule, "CGC_Rtp_Register_Source");
		CGC_Rtp_UnRegister_Source fp2 = (CGC_Rtp_UnRegister_Source)dlsym(hModule, "CGC_Rtp_UnRegister_Source");
		FPCGC_Rtp_Register_Sink fp3 = (FPCGC_Rtp_Register_Sink)dlsym(hModule, "CGC_Rtp_Register_Sink");
#endif
		portApp->setFuncHandle1((void*)fp1);
		portApp->setFuncHandle2((void*)fp2);
		portApp->setFuncHandle3((void*)fp3);
	}

	FPCGC_Rtp_Register_Source fp = (FPCGC_Rtp_Register_Source)portApp->getFuncHandle1();
	if (fp==NULL)
		return false;
	return fp(nRoomId, nSourceId, nParam);
}
void CGCApp::onUnRegisterSource(cgc::bigint nRoomId, cgc::bigint nSourceId, cgc::bigint nParam, void* pUserData)
{
	const int nServerPort = (int)pUserData;
	CPortApp::pointer portApp = m_parsePortApps.getPortApp(nServerPort);
	if (portApp.get() == NULL)
	{
		return;	// *
	}
	CGC_Rtp_UnRegister_Source fp = (CGC_Rtp_UnRegister_Source)portApp->getFuncHandle2();
	if (fp==NULL)
		return;
	fp(nRoomId, nSourceId, nParam);
}
bool CGCApp::onRegisterSink(cgc::bigint nRoomId, cgc::bigint nSourceId, cgc::bigint nDestId, void* pUserData)
{
	const int nServerPort = (int)pUserData;
	CPortApp::pointer portApp = m_parsePortApps.getPortApp(nServerPort);
	if (portApp.get() == NULL)
	{
		return true;	// *
	}
	FPCGC_Rtp_Register_Sink fp = (FPCGC_Rtp_Register_Sink)portApp->getFuncHandle3();
	if (fp==NULL)
		return false;
	return fp(nRoomId, nSourceId, nDestId);
}

tstring CGCApp::onGetSslPassword(const tstring& sSessionId) const
{
	cgcSession::pointer sessionImpl = m_mgrSession.GetSessionImpl(sSessionId);
	if (sessionImpl.get() == NULL)
	{
		return "";
	}
	CSessionImpl * pSessionImpl = (CSessionImpl*)sessionImpl.get();
	return pSessionImpl->GetSslPassword();
}

int CGCApp::onRemoteAccept(const cgcRemote::pointer& pcgcRemote)
{
	// 记录下新的 SockRemote
	//try
	{
		//m_mgrSession.GetSessionImplByRemote(pcgcRemote->getRemoteId());
		return 0;
	//}catch (const std::exception & e)
	//{
	//	m_logModuleImpl.log(LOG_ERROR, _T("exception, OnRemoteAccept(), \'%s\', lasterror=0x%x\n"), e.what(), GetLastError());
	//}catch (...)
	//{
	//	m_logModuleImpl.log(LOG_ERROR, _T("exception, OnRemoteAccept(), lasterror=0x%x\n"), GetLastError());
	}

	return -111;
}

typedef int (FAR *FPCGC_DefaultFunc)(const cgcRequest::pointer & pRequest, cgcResponse::pointer & pResponse);
//typedef int (FAR *FPCGC_DefaultFunc)(cgcRemote::pointer & pcgcRemote, const unsigned char * recvData, size_t recvLen);

int CGCApp::onRecvData(const cgcRemote::pointer& pcgcRemote, const unsigned char * recvData, size_t recvLen)
{
	if (m_bStopedApp || recvData == 0 || recvLen == 0 || pcgcRemote->isInvalidate())
	{
		printf("CGCApp::onRecvData isInvalidate size=%d\n",recvLen);
		return -1;
	}

	//printf("CGCApp::onRecvData:%d size=%d\n",pcgcRemote->getRemoteId(),recvLen);
	try
	{
		if (pcgcRemote->getProtocol() == PROTOCOL_SOTP || (pcgcRemote->getProtocol() & PROTOCOL_HSOTP))
		{
			if (m_fpParserSotpService == NULL) return -2;
			ProcCgcData(recvData, recvLen, pcgcRemote);
		}else if (pcgcRemote->getProtocol() & PROTOCOL_HTTP)
		{
			if (m_fpParserHttpService == NULL) return -2;
			ProcHttpData(recvData, recvLen, pcgcRemote);
		}else
		{
			// ??
			int serverPort = pcgcRemote->getServerPort();
			CPortApp::pointer portApp = m_parsePortApps.getPortApp(serverPort);
			if (portApp.get() == NULL)
			{
				pcgcRemote->invalidate();
				return 0;
			}
			if (portApp->getModuleHandle() == NULL)
			{
				ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(portApp->getApp());
				if (moduleItem.get() == NULL || moduleItem->getModuleHandle() == NULL)
				{
					pcgcRemote->invalidate();
					return 0;
				}
				void * hModule = moduleItem->getModuleHandle();
				portApp->setModuleHandle(hModule);

				tstring sFunc = portApp->getFunc().empty() ? "doPortAppFunction" : portApp->getFunc();
#ifdef WIN32
				FPCGC_DefaultFunc fp = (FPCGC_DefaultFunc)GetProcAddress((HMODULE)hModule, sFunc.c_str());
#else
				FPCGC_DefaultFunc fp = (FPCGC_DefaultFunc)dlsym(hModule, sFunc.c_str());
#endif
				portApp->setFuncHandle1((void*)fp);

				cgcSession::pointer sessionImpl = m_mgrSession.SetSessionImpl(moduleItem,pcgcRemote,cgcNullParserBaseService);
				//CSessionImpl * pSessionImpl = (CSessionImpl*)sessionImpl.get();
			}

			if (portApp->getFuncHandle1() == NULL)
			{
				pcgcRemote->invalidate();
				return 0;
			}
			cgcSession::pointer sessionImpl = m_mgrSession.GetSessionImplByRemote(pcgcRemote->getRemoteId());
			if (sessionImpl.get() == NULL)
			{
				ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(portApp->getApp());
				if (moduleItem.get() == NULL || moduleItem->getModuleHandle() == NULL)
				{
					pcgcRemote->invalidate();
					return 0;
				}
				sessionImpl = m_mgrSession.SetSessionImpl(moduleItem,pcgcRemote,cgcNullParserBaseService);
				CSessionImpl * pSessionImpl = (CSessionImpl*)sessionImpl.get();
			}else
			{
				CSessionImpl * pSessionImpl = (CSessionImpl*)sessionImpl.get();
				pSessionImpl->setDataResponseImpl("",pcgcRemote);
			}
			cgcRequest::pointer requestImpl(new CRequestImpl(pcgcRemote, cgcNullParserBaseService));
			cgcResponse::pointer responseImpl(new CResponseImpl(pcgcRemote, cgcNullParserBaseService));

			((CRequestImpl*)requestImpl.get())->setSession(sessionImpl);
			((CRequestImpl*)requestImpl.get())->setContent((const char*)recvData, recvLen);
			((CResponseImpl*)responseImpl.get())->setSession(sessionImpl);

			FPCGC_DefaultFunc fp = (FPCGC_DefaultFunc)portApp->getFuncHandle1();
			fp(requestImpl, responseImpl);
			//if (!sResponse.empty())
			//{
			//	pcgcRemote->sendData((const unsigned char*)sResponse.c_str(), sResponse.size());
			//}

			//((FPCGC_DefaultFunc)portApp->getFuncHandle())(pcgcRemote, recvData, recvLen);
		}
	}catch (const std::exception & e)
	{
		m_logModuleImpl.log(LOG_ERROR, _T("exception, \'%s\', lasterror=0x%x\n"), e.what(), GetLastError());
	}catch (...)
	{
		m_logModuleImpl.log(LOG_ERROR, _T("exception, lasterror=0x%x\n"), GetLastError());
	}
	return 0;
}

int CGCApp::onRemoteClose(unsigned long remoteId, int nErrorCode)
{
	try
	{
		m_mgrSession.onRemoteClose(remoteId,nErrorCode);
		//m_mgrSession.setInterval(remoteId, 10);	// 10分钟，这里不处理，由应用在CGC_Session_Open()自行设置
		return 0;
	}catch (const std::exception & e)
	{
		try{
			m_logModuleImpl.log(LOG_ERROR, _T("exception, OnRemoteClose(), '%s', lasterror=0x%x\n"), e.what(), GetLastError());
		}catch(...){}
	}catch (...)
	{
		m_logModuleImpl.log(LOG_ERROR, _T("exception, OnRemoteClose(), lasterror=0x%x\n"), GetLastError());
	}

	return -111;
}

cgcServiceInterface::pointer CGCApp::getService(const tstring & serviceName, const cgcValueInfo::pointer& parameter)
{
	ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(serviceName);
	if (moduleItem.get() == NULL || moduleItem->getModuleHandle() == NULL)
		return cgcNullServiceInterface;

	void * hModule = moduleItem->getModuleHandle();

	cgcApplication::pointer application;
	if (!m_mapOpenModules.find(hModule, application)) return cgcNullServiceInterface;
	if (((CModuleImpl*)application.get())->getModuleState() < 1)
	{
		if (!InitLibModule(application, moduleItem)) return cgcNullServiceInterface;
	}

	FPCGC_GetService fp = (FPCGC_GetService)moduleItem->getFpGetService();
//#ifdef WIN32
//	FPCGC_GetService fp = (FPCGC_GetService)GetProcAddress((HMODULE)hModule, "CGC_GetService");
//#else
//	FPCGC_GetService fp = (FPCGC_GetService)dlsym(hModule, "CGC_GetService");
//#endif

	cgcServiceInterface::pointer resultService;
	if (fp != NULL)
	{
		bool bException = false;
		try
		{
			fp(resultService, parameter);
		}catch (std::exception const & e)
		{
			bException = true;
			m_logModuleImpl.log(LOG_ERROR, _T("getService exception! serviceName=%s: %s\n"), serviceName.c_str(), e.what());
		}catch (...)
		{
			bException = true;
			m_logModuleImpl.log(LOG_ERROR, _T("getService exception! serviceName=%s\n"), serviceName.c_str());
		}

		if (resultService.get() != NULL)
		{
			m_mapServiceModule.insert(resultService, hModule);
			if (bException)
			{
				resetService(resultService);
				resultService.reset();
			}
		}
	}
	return resultService;
}

void CGCApp::resetService(const cgcServiceInterface::pointer & service)
{
	if (service.get() ==  NULL) return;

	void * hModule = NULL;
	if (!m_mapServiceModule.find(service, hModule, false)) return;

#ifdef WIN32
	FPCGC_ResetService fp = (FPCGC_ResetService)GetProcAddress((HMODULE)hModule, "CGC_ResetService");
#else
	FPCGC_ResetService fp = (FPCGC_ResetService)dlsym(hModule, "CGC_ResetService");
#endif

	if (fp != NULL)
	{
		try
		{
			fp(service);
		}catch (std::exception const & e)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("resetService exception! serviceName=%s: %s\n"), service->serviceName().c_str(), e.what());
		}catch (...)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("resetService exception! serviceName=%s\n"), service->serviceName().c_str());
		}
	}
	m_mapServiceModule.remove(service);
}

HTTP_STATUSCODE CGCApp::executeInclude(const tstring & url, const cgcHttpRequest::pointer & request, const cgcHttpResponse::pointer& response)
{
	BOOST_ASSERT (request.get() != NULL);
	BOOST_ASSERT (response.get() != NULL);

	cgcParserHttp::pointer phttpParser;

	cgcServiceInterface::pointer parserService;
	m_fpParserHttpService(parserService, cgcNullValueInfo);
	if (parserService.get() == NULL)
	{
		m_logModuleImpl.log(LOG_ERROR, _T("executeInclude: ParserHttpService GetParser error! %s\n"), url);
		return STATUS_CODE_500;
	}
	phttpParser = CGC_PARSERHTTPSERVICE_DEF(parserService);
	phttpParser->forward(request->getRequestURI());	// 用于记录前面全目录
	phttpParser->forward(url);

	cgcHttpRequest::pointer requestImpl(new CHttpRequestImpl(((CHttpRequestImpl*)request.get())->getRemote(), phttpParser));
	((CHttpRequestImpl*)requestImpl.get())->setSession(request->getSession());
	((CHttpRequestImpl*)requestImpl.get())->setAttributes(request->getAttributes(true));
	requestImpl->setPageAttributes(request->getPageAttributes());
	//((CHttpResponseImpl*)responseImpl.get())->setSession(sessionImpl);

	HTTP_STATUSCODE statusCode = STATUS_CODE_500;
	try
	{
		const int MAX_FORWARDS = 100;
		int count = 0;
		do
		{
			if (count++ == MAX_FORWARDS) break;
			if (((CHttpResponseImpl*)response.get())->getForward())
			{
				((CHttpResponseImpl*)response.get())->setForward(false);
				if (requestImpl->getSession().get() != NULL)
				{
					tstring sAppModuleName;
					if (phttpParser->isServletRequest())
						sAppModuleName = phttpParser->getModuleName();
					else
						sAppModuleName = m_sHttpServerName;
					if (!((CSessionImpl*)requestImpl->getSession().get())->existModuleItem(sAppModuleName))
					{
						ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(sAppModuleName);
						if (moduleItem.get() == NULL)
						{
							statusCode = STATUS_CODE_500;
							break;
						}
						cgcRemote::pointer pRemote = ((CHttpResponseImpl*)response.get())->getCgcRemote();
						if (((CSessionImpl*)requestImpl->getSession().get())->OnRunCGC_Session_Open(moduleItem,pRemote))
						{
							m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\'.%s opened\n"), requestImpl->getSession()->getId().c_str(),moduleItem->getName().c_str());
						}else
						{
							statusCode = STATUS_CODE_401;
							break;
							// 删除 sessionimpl
							//m_mgrSession.RemoveSessionImpl(sessionImpl);
						}
					}
				}
			}

			if (phttpParser->isServletRequest())
				statusCode = ProcHttpAppProto(requestImpl, response, phttpParser);
			else
				statusCode = m_fpHttpServer == NULL ? STATUS_CODE_503 : m_fpHttpServer(requestImpl, response);
		}while (statusCode == STATUS_CODE_200 && ((CHttpResponseImpl*)response.get())->getForward());

	}catch (std::exception const & e)
	{
		m_logModuleImpl.log(LOG_ERROR, _T("executeInclude exception! url=%s: %s\n"), url.c_str(), e.what());
	}catch (...)
	{
		m_logModuleImpl.log(LOG_ERROR, _T("executeInclude exception! url=%s\n"), url.c_str());
	}

	return statusCode;
}

HTTP_STATUSCODE CGCApp::executeService(const tstring & serviceName, const tstring& function, const cgcHttpRequest::pointer & request, const cgcHttpResponse::pointer& response, tstring & outExecuteResult)
{
	BOOST_ASSERT (request.get() != NULL);
	BOOST_ASSERT (response.get() != NULL);

	ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(serviceName);
	if (moduleItem.get() == NULL || moduleItem->getModuleHandle() == NULL)
		return STATUS_CODE_404;

	void * hModule = moduleItem->getModuleHandle();

	cgcApplication::pointer application;
	if (!m_mapOpenModules.find(hModule, application)) return STATUS_CODE_404;
	if (((CModuleImpl*)application.get())->getModuleState() < 1)
	{
		if (!InitLibModule(application, moduleItem)) return STATUS_CODE_501;
	}

	tstring doFunction("do");
	doFunction.append(function);

#ifdef WIN32
	FPCGCHttpApi fp = (FPCGCHttpApi)GetProcAddress((HMODULE)hModule, doFunction.c_str());
#else
	FPCGCHttpApi fp = (FPCGCHttpApi)dlsym(hModule, doFunction.c_str());
#endif

	HTTP_STATUSCODE result = fp == NULL ? STATUS_CODE_404 : STATUS_CODE_500;
	if (fp != NULL)
	{
		try
		{
			application->clearExecuteResult();
			result = fp(request, response);
			outExecuteResult = application->getExecuteResult();
		}catch (std::exception const & e)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("runService exception! serviceName=%s; %s: %s\n"), serviceName.c_str(), function.c_str(), e.what());
		}catch (...)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("runService exception! serviceName=%s; %s\n"), serviceName.c_str(), function.c_str());
		}
	}
	return result;
}

cgcCDBCService::pointer CGCApp::getCDBDService(const tstring& datasource)
{
	cgcCDBCService::pointer result;
	if (m_cdbcServices.find(datasource, result)) return result;
	
	cgcDataSourceInfo::pointer dataSourceInfo = m_cdbcs.getDataSourceInfo(datasource);
	if (dataSourceInfo.get() == NULL) return cgcNullCDBCService;

	cgcServiceInterface::pointer serviceInterface = getService(dataSourceInfo->getCDBCService());
	if (serviceInterface.get() == NULL) return cgcNullCDBCService;

	result = CGC_CDBCSERVICE_DEF(serviceInterface);
	if (!result->initService())
	{
		resetService(result);
		return cgcNullCDBCService;
	}

	if (!result->open(dataSourceInfo->getCDBCInfo()))
	{
		resetService(result);
		return cgcNullCDBCService;
	}

	m_cdbcServices.insert(datasource, result);
	return result;
}
void CGCApp::retCDBDService(cgcCDBCServicePointer& cdbcservice)
{
	if (cdbcservice.get()!=NULL)
	{
		// *** 如何设计，断开超时没用连接

		resetService(cdbcservice);
		cdbcservice.reset();
	}
}

cgcResponse::pointer CGCApp::getLastResponse(const tstring & sessionId,const tstring& moduleName) const
{
	cgcSession::pointer pSession = m_mgrSession.GetSessionImpl(sessionId);
	return pSession.get() == NULL ? cgcNullResponse : pSession->getLastResponse(moduleName);
}
cgcResponse::pointer CGCApp::getHoldResponse(const tstring& sessionId,unsigned long remoteId)
{
	cgcSession::pointer pSession = m_mgrSession.GetSessionImpl(sessionId);
	return pSession.get() == NULL ? cgcNullResponse : pSession->getHoldResponse(remoteId);
}

cgcAttributes::pointer CGCApp::getAttributes(bool create)
{
	if (create && m_attributes.get() == NULL)
		m_attributes = cgcAttributes::pointer(new AttributesImpl());
	return m_attributes;
}

cgcAttributes::pointer CGCApp::getAppAttributes(const tstring & appName) const
{
	ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(appName);
	if (moduleItem.get() == NULL || moduleItem->getModuleHandle() == NULL)
		return cgcNullAttributes;

	cgcApplication::pointer applicationImpl;
	m_mapOpenModules.find(moduleItem->getModuleHandle(), applicationImpl);
	return applicationImpl.get() == NULL ? cgcNullAttributes : applicationImpl->getAttributes();
}

tstring CGCApp::SetNewMySessionId(cgcParserHttp::pointer& phttpParser,const tstring& sSessionId)
{
	if (sSessionId.empty())
	{
		char lpszMySessionId[64];
		static unsigned int the_sid_index=0;
		sprintf(lpszMySessionId, "%d%d%d",(int)time(0),(int)rand(),(the_sid_index++));
		MD5 md5;
		md5.update((const unsigned char*)lpszMySessionId,strlen(lpszMySessionId));
		md5.finalize();
		const std::string sMySessionId = md5.hex_digest();
		phttpParser->setCookieMySessionId(sMySessionId);
	}else
	{
		phttpParser->setCookieMySessionId(sSessionId);
	}
	//phttpParser->setHeader("P3P","CP=\"CAO PSA OUR\"");	// 解决IE多窗口SESSION不正常问题
	//phttpParser->setHeader("P3P","\"policyref=\"http://test-lc.entboost.com/w3c/p3p.xml\" CP=\"ALL DSP COR CUR OUR IND PUR\"");
	return phttpParser->getCookieMySessionId();
}

void CGCApp::GetHttpParserPool(cgcParserHttp::pointer& phttpParser)
{
	if (m_pHttpParserPool.front(phttpParser))
	{
		phttpParser->init();
		return;
	}
	m_tLastNewParserHttpTime = time(0);
	cgcServiceInterface::pointer parserService;
	m_fpParserHttpService(parserService, cgcNullValueInfo);
	if (parserService.get() != NULL)
	{
		phttpParser = CGC_PARSERHTTPSERVICE_DEF(parserService);
	}
}
void CGCApp::SetHttpParserPool(const cgcParserHttp::pointer& phttpParser)
{
	//return;
	// *** 200 max pool size
	if (m_pHttpParserPool.size()<200)
	{
		m_pHttpParserPool.add(phttpParser);
	}
}
void CGCApp::CheckHttpParserPool(void)
{
	// *** 20 min pool size
	if (m_pHttpParserPool.size()>20 && (time(0)-m_tLastNewParserHttpTime)>5)
	{
		cgcParserHttp::pointer phttpParser;
		m_pHttpParserPool.front(phttpParser);
	}
}


HTTP_STATUSCODE CGCApp::ProcHttpData(const unsigned char * recvData, size_t dataSize,const cgcRemote::pointer& pcgcRemote)
{
	bool findMultiPartEnd = false;
	cgcParserHttp::pointer phttpParser;
	bool bCanSetParserToPool = false;

	cgcMultiPart::pointer currentMultiPart;
	if (m_mapMultiParts.find(pcgcRemote->getRemoteId(), currentMultiPart))
	{
		phttpParser = CGC_PARSERHTTPSERVICE_DEF(currentMultiPart->getParser());
	}
	if (phttpParser.get() == NULL)
	{
		GetHttpParserPool(phttpParser);
		bCanSetParserToPool = true;
		if (phttpParser.get() == NULL)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("ParserHttpService GetParser error! %s\n"), recvData);
			return STATUS_CODE_500;
		}
		//cgcServiceInterface::pointer parserService;
		//m_fpParserHttpService(parserService, cgcNullValueInfo);
		//if (parserService.get() == NULL)
		//{
		//	m_logModuleImpl.log(LOG_ERROR, _T("ParserHttpService GetParser error! %s\n"), recvData);
		//	return STATUS_CODE_500;
		//}
		//phttpParser = CGC_PARSERHTTPSERVICE_DEF(parserService);
	}
	cgcHttpRequest::pointer requestImpl(new CHttpRequestImpl(pcgcRemote, phttpParser));
	cgcHttpResponse::pointer responseImpl(new CHttpResponseImpl(pcgcRemote, phttpParser));
	((CHttpRequestImpl*)requestImpl.get())->setContent((const char*)recvData, dataSize);
	HTTP_STATUSCODE statusCode = STATUS_CODE_200;
	bool parseResult = phttpParser->doParse(recvData, dataSize);
	if (!parseResult)
	{
		cgcMultiPart::pointer multiPart = phttpParser->getMultiPart();
		//printf("******** doParse false:%d, multiparth=%d\n",pcgcRemote->getRemoteId(),multiPart.get()==NULL?0:1);
		if (multiPart.get() != NULL)
		{
			if (currentMultiPart.get() != NULL && currentMultiPart.get() != multiPart.get())
			{
				m_mapMultiParts.remove(pcgcRemote->getRemoteId());
				currentMultiPart.reset();
			}
			if (currentMultiPart.get() == NULL)
			{
				multiPart->setParser(phttpParser);
				//printf("******** m_mapMultiParts.insert:%d\n",pcgcRemote->getRemoteId());
				m_mapMultiParts.insert(pcgcRemote->getRemoteId(), multiPart);
			}
			return STATUS_CODE_100;	// ***不需要返回，客户端会一直上传；
		}else
		{
			// 
			statusCode = phttpParser->getStatusCode();
			if (statusCode==STATUS_CODE_200)
				statusCode = STATUS_CODE_501;
			m_logModuleImpl.log(LOG_WARNING, _T("ProcHttpData doParse false: state=%d:%d=%s\n"), (int)statusCode,dataSize,recvData);
		}
	}else if (parseResult)
	{
		// Parse OK.
		//printf("******** doParse true:%d\n",pcgcRemote->getRemoteId());
		if (currentMultiPart.get() != NULL)
		{
			//printf("******** m_mapMultiParts.remove:%d\n",pcgcRemote->getRemoteId());
			m_mapMultiParts.remove(pcgcRemote->getRemoteId());
			currentMultiPart.reset();
		}

		cgcSession::pointer sessionImpl;
		if (phttpParser->isEmptyCookieMySessionId())
		{
			sessionImpl = m_mgrSession.GetSessionImplByRemote(pcgcRemote->getRemoteId());
			if (sessionImpl.get()!=NULL)
				SetNewMySessionId(phttpParser,sessionImpl->getId());
			else
				SetNewMySessionId(phttpParser);
		}else
		{
			sessionImpl = m_mgrSession.GetSessionImpl(phttpParser->getCookieMySessionId());
			if (sessionImpl.get()==NULL)
			{
				printf("**** Can not find Session'%s',changed!\n", phttpParser->getCookieMySessionId().c_str());
				SetNewMySessionId(phttpParser);
			}
		}

		/*
		cgcSession::pointer sessionImpl = m_mgrSession.GetSessionImplByRemote(pcgcRemote->getRemoteId());
		if (phttpParser->isEmptyCookieMySessionId())
		{
			const tstring sSessionId = sessionImpl.get()==NULL?"":sessionImpl->getId();	// add by hd 2014-10-29
			SetNewMySessionId(phttpParser,sSessionId);
		}else if (sessionImpl.get()==NULL)	// add by hd 2014-10-29
		//}else
		{
			sessionImpl = m_mgrSession.GetSessionImpl(phttpParser->getCookieMySessionId());
			if (sessionImpl.get()==NULL)
			{
				printf("**** Can not find Session'%s',changed!\n", phttpParser->getCookieMySessionId().c_str());
				SetNewMySessionId(phttpParser);
			}
		}else if (sessionImpl.get()!=NULL && sessionImpl->getId()!=phttpParser->getCookieMySessionId())
		{
			// add by hd 2014-10-29
			printf("**** Find old session '%s',changed to '%s'!\n",phttpParser->getCookieMySessionId().c_str(),sessionImpl->getId().c_str());
			SetNewMySessionId(phttpParser,sessionImpl->getId());
		}
		*/

		CSessionImpl * pHttpSessionImpl = (CSessionImpl*)sessionImpl.get();
		if (phttpParser->getStatusCode()==STATUS_CODE_200)
			m_mgrSession.SetRemoteSession(pcgcRemote->getRemoteId(),phttpParser->getCookieMySessionId()); 

		// ****解决IE多窗口COOKIE丢失问题
		phttpParser->setHeader("P3P","CP=\"CAO PSA OUR\"");

		tstring sAppModuleName;
		if (phttpParser->isServletRequest())
		{
			sAppModuleName = phttpParser->getModuleName();
			CServletInfo::pointer servletInfo = m_parseWeb.setServletInfo(sAppModuleName);
			if (servletInfo.get() != NULL)
			{
				sAppModuleName = servletInfo->getServletApp();
			}
		}else
		{
			sAppModuleName = m_sHttpServerName;
		}
		if (sessionImpl.get() == NULL || !pHttpSessionImpl->existModuleItem(sAppModuleName))
		{
			// 新SESSION，或者换MODULE
			ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(sAppModuleName);
			if (moduleItem.get() != NULL)
			{
				if (sessionImpl.get() == NULL)
				{
					bCanSetParserToPool = false;
					sessionImpl = m_mgrSession.SetSessionImpl(moduleItem,pcgcRemote,phttpParser);
					pHttpSessionImpl = (CSessionImpl*)sessionImpl.get();
				}
				if (pHttpSessionImpl->OnRunCGC_Session_Open(moduleItem,pcgcRemote))
				{
					m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\'.%s opened\n"), sessionImpl->getId().c_str(),moduleItem->getName().c_str());
				}else
				{
					statusCode = STATUS_CODE_401;
					sessionImpl.reset();
					pHttpSessionImpl = NULL;
				}
			}
		}

		if (sessionImpl.get() != NULL)
		{
			pHttpSessionImpl->setDataResponseImpl(sAppModuleName,pcgcRemote);
		}
//		if (sessionImpl.get() == NULL)
//		{
//			// 这段代码没用了
////			unsigned long remoteId = pcgcRemote->getRemoteId();
////			bool bOpenSession = false;
////			if (m_mapRemoteOpenSes.find(remoteId, bOpenSession))
////			{
////				// Wait for open the session
////#ifdef WIN32
////				Sleep(100);
////#else
////				usleep(100000);
////#endif
////			}else
////			{
////				m_mapRemoteOpenSes.insert(remoteId, true);
////			}
////			sessionImpl = m_mgrSession.GetSessionImplByRemote(remoteId);
////
////			// 客户端没有OPEN SESSION, sSessionId=APPNAME.
////			// 临时打开SESSION
////			if (sessionImpl.get() == NULL)
////			{
////				tstring sAppModuleName;
////				if (phttpParser->isServletRequest())
////					sAppModuleName = phttpParser->getModuleName();
////				else
////					sAppModuleName = m_sHttpServerName;
////				ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(sAppModuleName);
////				if (moduleItem.get() != 0)
////				{
////					//cgcServiceInterface::pointer parserService;
////					//m_fpParserHttpService(parserService, cgcNullValueInfo);
////					//if (parserService.get() == NULL) return STATUS_CODE_500;
////					//sessionImpl = m_mgrSession.SetSessionImpl(pcgcRemote, pcgcParserLastSession);
////					sessionImpl = m_mgrSession.SetSessionImpl(pcgcRemote, phttpParser);
////					pHttpSessionImpl = (CSessionImpl*)sessionImpl.get();
////					// 调用模块 CGC_Session_Open
////					if (pHttpSessionImpl->OnRunCGC_Session_Open(moduleItem))
////					{
////						m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\'.%s opened\n"), sessionImpl->getId().c_str(),moduleItem->getName().c_str());
////					}else
////					{
////						// 删除 sessionimpl
////						//m_mgrSession.RemoveSessionImpl(sessionImpl);
////						statusCode = STATUS_CODE_401;
////						sessionImpl.reset();
////						pHttpSessionImpl = NULL;
////					}
////				}
////			}
////			m_mapRemoteOpenSes.remove(remoteId);
//		}else
//		{
//			// Record cgcRemote
//			pHttpSessionImpl->setDataResponseImpl(sAppModuleName,pcgcRemote);
//		}

		try
		{
			if (sessionImpl.get() != NULL)
			{
				if (phttpParser->getKeepAlive() > 0)
					sessionImpl->setMaxInactiveInterval(phttpParser->getKeepAlive());

				//if (m_fpHttpStruct != NULL)
				//	m_fpHttpStruct(requestImpl, responseImpl);
				const int MAX_FORWARDS = 100;
				int count = 0;
				do
				{
					if (count++ == MAX_FORWARDS) break;
					if (((CHttpResponseImpl*)responseImpl.get())->getForward())
					{
						((CHttpResponseImpl*)responseImpl.get())->setForward(false);

						tstring sAppModuleName;
						if (phttpParser->isServletRequest())
						{
							sAppModuleName = phttpParser->getModuleName();
							CServletInfo::pointer servletInfo = m_parseWeb.setServletInfo(sAppModuleName);
							if (servletInfo.get() != NULL)
							{
								sAppModuleName = servletInfo->getServletApp();
							}
						}else
						{
							sAppModuleName = m_sHttpServerName;
						}
						if (!pHttpSessionImpl->existModuleItem(sAppModuleName))
						{
							ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(sAppModuleName);
							if (moduleItem.get() == NULL)
							{
								statusCode = STATUS_CODE_500;
								break;
							}
							if (pHttpSessionImpl->OnRunCGC_Session_Open(moduleItem,pcgcRemote))
							{
								m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\'.%s opened\n"), sessionImpl->getId().c_str(),moduleItem->getName().c_str());
							}else
							{
								statusCode = STATUS_CODE_401;
								break;
								// 删除 sessionimpl
								//m_mgrSession.RemoveSessionImpl(sessionImpl);
							}
						}
					}

					((CHttpRequestImpl*)requestImpl.get())->setSession(sessionImpl);
					((CHttpResponseImpl*)responseImpl.get())->setSession(sessionImpl);
					if (phttpParser->isServletRequest())
						statusCode = ProcHttpAppProto(requestImpl, responseImpl, phttpParser);
					else
						statusCode = m_fpHttpServer == NULL ? STATUS_CODE_503 : m_fpHttpServer(requestImpl, responseImpl);
				}while (statusCode == STATUS_CODE_200 && ((CHttpResponseImpl*)responseImpl.get())->getForward());
			}
		}catch(std::exception const &e)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("exception, RequestURL \'%s\', lasterror=0x%x\n"),
				phttpParser->getRequestURL().c_str(),
				GetLastError());
			m_logModuleImpl.log(LOG_ERROR, _T("'%s'\n"), e.what());
			statusCode = STATUS_CODE_500;
		}catch(...)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("exception, RequestURL \'%s\', lasterror=0x%x\n"),
				phttpParser->getRequestURL().c_str(),
				GetLastError());
			statusCode = STATUS_CODE_500;
		}
	}else
	{
		//if (pHttpSessionImpl)
		//{
		//	// 第一次处理不成功，保存未处理数据
		//	if (!pHttpSessionImpl->isHasPrevData())
		//	{
		//		pHttpSessionImpl->delPrevDatathread(false);
		//		pHttpSessionImpl->addPrevData((const char*)recvData);

		//		// 创建一个超时处理线程
		//		pHttpSessionImpl->newPrevDataThread();
		//		statusCode = STATUS_CODE_100;
		//	}else
		//	{
		//		//pHttpSessionImpl->addPrevData((const char*)recvData);
		//		statusCode = STATUS_CODE_400;
		//	}
		//}else
		{
			// 之前没有SESSION，直接返回错误
			statusCode = STATUS_CODE_400;
		}
	}

	//if (statusCode != STATUS_CODE_200)
	{
		responseImpl->setStatusCode(statusCode);
		//if (responseImpl->getBodySize() == 0)
		//	responseImpl->println(cgcGetStatusCode(statusCode));
	}
	//if (statusCode!=STATUS_CODE_200)
	//	responseImpl->setKeepAlive(-1);
	// 自动发送
	((CHttpResponseImpl*)responseImpl.get())->sendResponse();
	//if (statusCode != STATUS_CODE_200)
	//if (statusCode == STATUS_CODE_413)
	if (!requestImpl->isKeepAlive())
	{
		m_pNotKeepAliveRemoteList.add(CNotKeepAliveRemote::create(pcgcRemote));
	}
	
	if (bCanSetParserToPool)
		SetHttpParserPool(phttpParser);
	return statusCode;
}


void CGCApp::GetSotpParserPool(cgcParserSotp::pointer& pcgcParser)
{
	if (m_pSotpParserPool.front(pcgcParser))
	{
		pcgcParser->init();
		return;
	}
	m_tLastNewParserSotpTime = time(0);
	cgcServiceInterface::pointer parserService;
	m_fpParserSotpService(parserService, cgcNullValueInfo);
	if (parserService.get() != NULL)
		pcgcParser = CGC_PARSERSOTPSERVICE_DEF(parserService);
}
void CGCApp::SetSotpParserPool(const cgcParserSotp::pointer& pcgcParser)
{
	//return;
	// *** 200 max pool size
	if (m_pSotpParserPool.size()<200)
	{
		m_pSotpParserPool.add(pcgcParser);
	}
}
void CGCApp::CheckSotpParserPool(void)
{
	// *** 20 min pool size
	//printf("** m_pSotpParserPool.size()=%d\n",m_pSotpParserPool.size());
	if (m_pSotpParserPool.size()>20 && (time(0)-m_tLastNewParserSotpTime)>5)
	{
		cgcParserSotp::pointer pcgcParser;
		m_pSotpParserPool.front(pcgcParser);
	}
}
void CGCApp::ProcCheckParserPool(void)
{
	static unsigned int theIndex = 0;
	theIndex++;
	if ((theIndex%3)==2)
	{
		CheckSotpParserPool();
		CheckHttpParserPool();
	}
}


int CGCApp::ProcCgcData(const unsigned char * recvData, size_t dataSize, const cgcRemote::pointer& pcgcRemote)
{
	cgcParserSotp::pointer pcgcParser;
	GetSotpParserPool(pcgcParser);
	if (pcgcParser.get() == NULL)
	//cgcServiceInterface::pointer parserService;
	//m_fpParserSotpService(parserService, cgcNullValueInfo);
	//if (parserService.get() == NULL)
	{
		m_logModuleImpl.log(LOG_ERROR, _T("ParserSotpService GetParser error! %s\n"), recvData);
		return -1;
	}

	//cgcParserSotp::pointer pcgcParser = CGC_PARSERSOTPSERVICE_DEF(parserService);
	pcgcParser->setParseCallback(this);
	const bool parseResult = pcgcParser->doParse(recvData, dataSize);
	if (pcgcParser->getProtoType()==SOTP_PROTO_TYPE_RTP)
	{
		if (pcgcParser->isRtpCommand())
		{
			//tagSotpRtpCommand pRtpCommand = pcgcParser->getRtpCommand();
			//printf("**** command=%d,roomid=%lld,srcid=%lld,size=%d\n",pRtpCommand.m_nCommand,pRtpCommand.m_nRoomId, pRtpCommand.m_nSrcId,SOTP_RTP_COMMAND_SIZE);
			const int serverPort = pcgcRemote->getServerPort();
			CSotpRtpSession::pointer pRtpSession;
			if (!m_pRtpSession.find(serverPort,pRtpSession))
			{
				pRtpSession = CSotpRtpSession::create(true);
				CSotpRtpSession::pointer pRtpSessionTemp;
				m_pRtpSession.insert(serverPort, pRtpSession, false, &pRtpSessionTemp);
				if (pRtpSessionTemp.get()!=NULL)
					pRtpSession = pRtpSessionTemp;
			}
			pRtpSession->doRtpCommand(pcgcParser->getRtpCommand(),pcgcRemote,false,this,(void*)serverPort);
		}else if (pcgcParser->isRtpData())
		{
			const int serverPort = pcgcRemote->getServerPort();
			CSotpRtpSession::pointer pRtpSession;
			if (m_pRtpSession.find(serverPort,pRtpSession))
			{
				pRtpSession->doRtpData(pcgcParser->getRtpDataHead(),pcgcParser->getRecvAttachment(),pcgcRemote);
			}

			//tagSotpRtpDataHead pRtpDataHead = pcgcParser->getRtpDataHead();
			////printf("**** data-head : roomid=%lld,srcid=%lld,size=%d\n",pRtpDataHead.m_nRoomId,pRtpDataHead.m_nSrcId,SOTP_RTP_DATA_HEAD_SIZE);
			//m_pRtpSession.doRtpData(pcgcParser->getRtpDataHead(),pcgcParser->getRecvAttachment(),pcgcRemote);
		}
		SetSotpParserPool(pcgcParser);
		return 1;
	}

	cgcSession::pointer sessionImpl = m_mgrSession.GetSessionImplByRemote(pcgcRemote->getRemoteId());
	cgcSotpRequest::pointer requestImpl(new CSotpRequestImpl(pcgcRemote, pcgcParser));

	//
	CSotpRequestImpl * pRequestImpl = (CSotpRequestImpl*)requestImpl.get();
	pRequestImpl->setContent((const char*)recvData, dataSize);
	CSessionImpl * pSessionImpl = (CSessionImpl*)sessionImpl.get();
	if (pSessionImpl)
	{
		((CSotpRequestImpl*)requestImpl.get())->setSession(sessionImpl);
		// Record cgcRemote
		pSessionImpl->setDataResponseImpl("",pcgcRemote);
	}

	//bool parseResult = false;
	//pcgcParser->setParseCallback(this);
	//if (pSessionImpl != NULL && pSessionImpl->isHasPrevData())
	//{
	//	// 之前有未处理数据
	//	const std::string & sRecvData = pSessionImpl->addPrevData((const char*)recvData);
	//	parseResult = pcgcParser->doParse((const unsigned char*)sRecvData.c_str(), sRecvData.size());
	//}else
	//{
	//	parseResult = pcgcParser->doParse(recvData, dataSize);
	//}

	//printf("**** doParse ok=%d\n",parseResult?1:0);

	if (parseResult)
	{
		// 成功解析
		//printf("%s\n", recvData);
		if (pcgcParser->hasSeq())
		{
			short seq = pcgcParser->getSeq();
			if (pcgcParser->isAckProto())
			{
				if (pSessionImpl)
					pSessionImpl->ProcessAckSeq(seq);
				SetSotpParserPool(pcgcParser);
				return 1;
			}
			if (pcgcParser->isNeedAck())
			{
				const tstring responseData = pcgcParser->getAckResult(seq);
				pcgcRemote->sendData((const unsigned char*)responseData.c_str(), responseData.length());
			}
			size_t nContentLength = dataSize;
			if (pSessionImpl)
			{
				if (pSessionImpl->isHasPrevData())
					nContentLength = pSessionImpl->getPrevDataLength();
				pSessionImpl->clearPrevData();

				//printf("seq=%d,sid=%s\n", seq, sessionImpl->getId().c_str());
				if (pSessionImpl->ProcessDuplationSeq(seq))
				{
					//printf("seq=%d,duplation \n", seq);
					SetSotpParserPool(pcgcParser);
					return 1;
				}
			}
		}
		try
		{
			if (pcgcParser->isSessionProto())
			{
				ProcSesProto(requestImpl, pcgcParser, pcgcRemote, sessionImpl);
			}else if (pcgcParser->isAppProto())
			{
				cgcSotpResponse::pointer responseImpl(new CSotpResponseImpl(pcgcRemote, pcgcParser, pSessionImpl));
				responseImpl->setEncoding(m_parseDefault.getDefaultEncoding());
				((CSotpResponseImpl*)responseImpl.get())->setSession(sessionImpl);
				ProcAppProto(requestImpl, responseImpl, pcgcParser, sessionImpl);
			}/* ? else if (pRequestImpl->m_cwsInvoke.isClusterProto())
			{
				ProcCluProto(pRequestImpl, cwssRemote);
			}*/else
			{
				m_logModuleImpl.log(LOG_ERROR, _T("invalidate cgc proto!\n"));
				//retCode = -115;
			}
		}catch(std::exception const &e)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("exception, sid \'%s\', cid \'%d\', cs \'%d\', lasterror=0x%x\n"),
				pcgcParser->getSid().c_str(),
				pcgcParser->getCallid(),
				pcgcParser->getSign(),
				GetLastError());
			m_logModuleImpl.log(LOG_ERROR, _T("'%s'\n"), e.what());
			//retCode = -111;
		}catch(...)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("exception, sid \'%s\', cid \'%d\', cs \'%d\', lasterror=0x%x\n"),
				pcgcParser->getSid().c_str(),
				pcgcParser->getCallid(),
				pcgcParser->getSign(),
				GetLastError());
			//retCode = -111;
		}
	}else
	{
		if (pSessionImpl)
		{
			// 第一次处理不成功，保存未处理数据
			if (!pSessionImpl->isHasPrevData())
			{
				pSessionImpl->delPrevDatathread(false);
				pSessionImpl->addPrevData((const char*)recvData);

				// 创建一个超时处理线程
				pSessionImpl->newPrevDataThread();
			}
		}else
		{
			// 之前没有SESSION，直接返回XML错误
			CSotpResponseImpl responseImpl(pcgcRemote, pcgcParser, pSessionImpl);
			responseImpl.SetEncoding(m_parseDefault.getDefaultEncoding());
			responseImpl.sendAppCallResult(-112);
		}
	}

	SetSotpParserPool(pcgcParser);
	return parseResult ? 0 : -1;
}

int CGCApp::ProcSesProto(const cgcSotpRequest::pointer& pRequestImpl, const cgcParserSotp::pointer& pcgcParser, const cgcRemote::pointer& pcgcRemote, cgcSession::pointer& remoteSessionImpl)
{
	if (!m_bInitedApp || m_bStopedApp || pcgcParser.get() == NULL) return -1;

	if (pcgcRemote->isInvalidate()) return -1;
	if (!pcgcParser->isSessionProto()) return -1;

	long retCode = 0;
	tstring sSessionId;
	unsigned long nCallId = pcgcParser->getCallid();

	CSessionImpl * pRemoteSessionImpl = (CSessionImpl*)remoteSessionImpl.get();
	// session
	if (pcgcParser->isOpenType())
	{
		// session open
		const tstring & sAppModuleName = pcgcParser->getModuleName();

		m_logModuleImpl.log(LOG_DEBUG, _T("SESSION OPENING..., APP_NAME=%s\n"), sAppModuleName.c_str());

		cgcApplication::pointer applicationImpl;
		ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(sAppModuleName);
		if (moduleItem.get() == NULL)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("no allow module \'%s\'!\n"), sAppModuleName.c_str());
			retCode = -101;
		}else if (moduleItem->getModuleHandle() == NULL)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("invalidate module handle \'%s\'!\n"), sAppModuleName.c_str());
			retCode = -102;
		}else if (!moduleItem->authAccount(pcgcParser->getAccount(), pcgcParser->getSecure()))
		{
			m_logModuleImpl.log(LOG_ERROR, _T("'%s' account auth failed!\n"), pcgcParser->getAccount().c_str());
			retCode = -104;
		}else if (!m_mapOpenModules.find(moduleItem->getModuleHandle(), applicationImpl) || !applicationImpl->isInited())
		{
			retCode = -105;
		}else
		{
			cgcParserSotp::pointer pcgcParserLastSession;
			GetSotpParserPool(pcgcParserLastSession);
			if (pcgcParserLastSession.get() == NULL) return -1;
			//cgcServiceInterface::pointer parserService;
			//m_fpParserSotpService(parserService, cgcNullValueInfo);
			//if (parserService.get() == NULL) return -1;
			//cgcParserSotp::pointer pcgcParserLastSession = CGC_PARSERSOTPSERVICE_DEF(parserService);
			if (NULL == pRemoteSessionImpl)
			{
				remoteSessionImpl = m_mgrSession.SetSessionImpl(moduleItem,pcgcRemote,pcgcParserLastSession);
				pRemoteSessionImpl = (CSessionImpl*)remoteSessionImpl.get();
			}

			//pRemoteSessionImpl->setAccontInfo(pcgcParser->getAccount(), pcgcParser->getSecure());
			sSessionId = remoteSessionImpl->getId();
			// call CGC_Session_Open
			if (pRemoteSessionImpl->OnRunCGC_Session_Open(moduleItem,pcgcRemote))
			{
				retCode = 200;
				m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\'.%s opened\n"), sSessionId.c_str(),moduleItem->getName().c_str());
				pRemoteSessionImpl->SetSslPublicKey(pcgcParser->getSslPublicKey());
			}else
			{
				// delete sessionimpl
				m_mgrSession.RemoveSessionImpl(remoteSessionImpl);
				retCode = -105;
			}
		}
	}else if (pcgcParser->isCloseType())
	{
		// session close
		sSessionId = pcgcParser->getSid();
		if (NULL == remoteSessionImpl.get())
		{
			remoteSessionImpl = m_mgrSession.GetSessionImpl(sSessionId);
			if (remoteSessionImpl.get()!=NULL)
			{
				m_mgrSession.SetRemoteSession(pcgcRemote->getRemoteId(),sSessionId);
				((CSessionImpl*)remoteSessionImpl.get())->setDataResponseImpl("",pcgcRemote);
			}
		}
		pRemoteSessionImpl = (CSessionImpl*)remoteSessionImpl.get();
		retCode = (pRemoteSessionImpl == NULL) ? -103 : 0;
	}else if (pcgcParser->isActiveType())
	{
		// active session
		sSessionId = pcgcParser->getSid();
		if (NULL == remoteSessionImpl.get())
		{
			remoteSessionImpl = m_mgrSession.GetSessionImpl(sSessionId);
			if (remoteSessionImpl.get()!=NULL)
			{
				m_mgrSession.SetRemoteSession(pcgcRemote->getRemoteId(),sSessionId);
				((CSessionImpl*)remoteSessionImpl.get())->setDataResponseImpl("",pcgcRemote);
				((CSessionImpl*)remoteSessionImpl.get())->OnRunCGC_Remote_Change(pcgcRemote);
			}
		}
		pRemoteSessionImpl = (CSessionImpl*)remoteSessionImpl.get();
		retCode = (pRemoteSessionImpl == NULL) ? -103 : 0;
	}else
	{
		m_logModuleImpl.log(LOG_ERROR, _T("invalidate cgc proto type \'%d\'!\n"), pcgcParser->getProtoType());
		retCode = -116;
	}

	CSotpResponseImpl responseImpl(pcgcRemote, pcgcParser, pRemoteSessionImpl);
	responseImpl.setSession(remoteSessionImpl);
	responseImpl.SetEncoding(m_parseDefault.getDefaultEncoding());
	// **暂时不需要返回public key
	//if (pRemoteSessionImpl!=NULL)
	//{
	//	responseImpl.SetSslPublicKey(m_pRsa.GetPublicKey());
	//}
	responseImpl.sendSessionResult(retCode, sSessionId);

	if (pcgcParser->isCloseType() && pRemoteSessionImpl != NULL)
	{
		m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\' closed\n"), sSessionId.c_str());
		remoteSessionImpl->invalidate();
		m_mgrSession.RemoveSessionImpl(remoteSessionImpl);
	}
	return 0;
}

HTTP_STATUSCODE CGCApp::ProcHttpAppProto(const cgcHttpRequest::pointer& requestImpl,const cgcHttpResponse::pointer& responseImpl,const cgcParserHttp::pointer& pcgcParser)
{
	assert (requestImpl.get() != NULL);
	assert (requestImpl->getSession() != NULL);
	assert (responseImpl.get() != NULL);
	assert (pcgcParser.get() != NULL);

	if (!m_bInitedApp || m_bStopedApp) return STATUS_CODE_503;
	//if (responseImpl->isInvalidate()) return STATUS_CODE_503;

	cgcSession::pointer remoteSessionImpl = requestImpl->getSession();
	CSessionImpl * pHttpRemoteSessionImpl = (CSessionImpl*)remoteSessionImpl.get();
	
	HTTP_STATUSCODE statusCode = STATUS_CODE_200;
	const tstring & sCallName = pcgcParser->getFunctionName();
	tstring methodName(sCallName);
	tstring sSessionId(remoteSessionImpl->getId());

	tstring sAppModuleName;
	if (pcgcParser->isServletRequest())
		sAppModuleName = pcgcParser->getModuleName();
	else
		sAppModuleName = m_sHttpServerName;
	ModuleItem::pointer pModuleItem = pHttpRemoteSessionImpl->getModuleItem(sAppModuleName,true);
	assert (pModuleItem.get() != NULL);
	if (!pModuleItem->getAllowMethod(sCallName, methodName))
	{
		m_logModuleImpl.log(LOG_ERROR, _T("no allow to call \'%s\'!\n"), sCallName.c_str());
		statusCode = STATUS_CODE_403;
	}else
	{
		if (pModuleItem->getType() == MODULE_SERVER && pModuleItem->getAllowAll() && !pModuleItem->getParam().empty())
			methodName = pModuleItem->getParam();

		m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\' call '%s.%s.%s'\n"), sSessionId.c_str(),requestImpl->getRestVersion().c_str(),pModuleItem->getName().c_str(), methodName.c_str());

		///////////// app call /////////////////
		boost::mutex::scoped_lock * lockWait = NULL;
		CModuleImpl * pModuleImpl = NULL;
		cgcApplication::pointer applicationImpl;
		if (!m_mapOpenModules.find(pModuleItem->getModuleHandle(), applicationImpl) || !applicationImpl->isInited())
		{
			m_logModuleImpl.log(LOG_ERROR, _T("invalidate module handle \'%s\'!\n"), pModuleItem->getName().c_str());
			statusCode = STATUS_CODE_500;
		}else
		{
			pModuleImpl = (CModuleImpl*)applicationImpl.get();

/*			// lock state
			if (pModuleItem->getLockState() == ModuleItem::LS_WAIT)
			{
				// callref
				pModuleImpl->addCallRef();

				// lock
				lockWait = new boost::mutex::scoped_lock(pModuleImpl->m_mutexCallWait);
			}else if (pModuleItem->getLockState() == ModuleItem::LS_RETURN)
			{
				if (pModuleImpl->getCallRef() == 0)
				{
					// callref
					pModuleImpl->addCallRef();
				}else
				{
					m_logModuleImpl.log(LOG_ERROR, _T("module \'%s\' lock return = \'%d\'!\n"), pModuleItem->getName().c_str(), pModuleImpl->getCallRef());
					//statusCode = -107;
				}
			}else
			{
				// callref
				pModuleImpl->addCallRef();
			}
			*/
		}

		if (statusCode == STATUS_CODE_200)
		{
			try
			{
				statusCode = ProcHttpLibMethod(pModuleItem, methodName, requestImpl, responseImpl);
			}catch(std::exception const & e)
			{
				m_logModuleImpl.log(LOG_ERROR, _T("exception, sessionid=\'%s\', callname=\'%s\', lasterror=0x%x\n"), sSessionId.c_str(), methodName.c_str(), GetLastError());
				m_logModuleImpl.log(LOG_ERROR, _T("'%s'\n"), e.what());
				statusCode = STATUS_CODE_500;
			}catch(...)
			{
				m_logModuleImpl.log(LOG_ERROR, _T("exception, sessionid=\'%s\', callname=\'%s\', lasterror=0x%x\n"), sSessionId.c_str(), methodName.c_str(), GetLastError());
				statusCode = STATUS_CODE_500;
			}
			m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\' returnCode '%d'\n"), sSessionId.c_str(), statusCode);

			// callref
			//if (pModuleImpl)
			//	pModuleImpl->releaseCallRef();
		}

		// unlock
		//if (lockWait)
		//	delete lockWait;
	}
	return statusCode;
}

int CGCApp::ProcAppProto(const cgcSotpRequest::pointer& requestImpl, const cgcSotpResponse::pointer& responseImpl, const cgcParserSotp::pointer& pcgcParser,cgcSession::pointer& remoteSessionImpl)
{
	if (!m_bInitedApp || m_bStopedApp) return -1;
	if (responseImpl->isInvalidate()) return -1;
	if (!pcgcParser->isAppProto()) return -1;

	CSotpResponseImpl * pResponseImpl = (CSotpResponseImpl*)responseImpl.get();
	CSessionImpl * pRemoteSessionImpl = (CSessionImpl*)remoteSessionImpl.get();
	bool bOpenNewSession = false;
	long retCode = 0;
	if (pcgcParser->isCallType())
	{
		const unsigned long nCallId = pcgcParser->getCallid();
		const unsigned long nSign = pcgcParser->getSign();

		const tstring & sCallName = pcgcParser->getFunctionName();
		tstring methodName(sCallName);
		tstring sSessionId = pcgcParser->getSid();
		if (pRemoteSessionImpl == NULL && !sSessionId.empty())
		{
			//printf("**** ProcAppProto Session NULL sid=%s\n",sSessionId.c_str());
			remoteSessionImpl = m_mgrSession.GetSessionImpl(sSessionId);
			pRemoteSessionImpl = (CSessionImpl*)remoteSessionImpl.get();
			//printf("**** ProcAppProto Session NULL sid=%s,session=%d\n",sSessionId.c_str(),(int)pRemoteSessionImpl);
			if (pRemoteSessionImpl!=NULL)
			{
				const cgcRemote::pointer pcgcRemote = pResponseImpl->getCgcRemote();
				m_mgrSession.SetRemoteSession(pcgcRemote->getRemoteId(),sSessionId);
				pRemoteSessionImpl->setDataResponseImpl("",pcgcRemote);
				pRemoteSessionImpl->OnRunCGC_Remote_Change(pcgcRemote);
			}else
			{
				retCode = -103;
			}
		}
		if (retCode==0 && pRemoteSessionImpl == NULL)
		{
			unsigned long remoteId = 0;
			cgcRemote::pointer pcgcRemote = pResponseImpl->getCgcRemote();
			if (pcgcRemote != NULL)
			{
				remoteId = pcgcRemote->getRemoteId();
				bool bOpenSession = false;
				if (m_mapRemoteOpenSes.find(remoteId, bOpenSession, false))
				{
					// Wait for open the session
#ifdef WIN32
					Sleep(100);
#else
					usleep(100000);
#endif
				}else
				{
					m_mapRemoteOpenSes.insert(remoteId, true);
				}
				remoteSessionImpl = m_mgrSession.GetSessionImplByRemote(remoteId);
				pRemoteSessionImpl = (CSessionImpl*)remoteSessionImpl.get();
			}

			//
			// 客户端没有OPEN SESSION, sSessionId=APPNAME.
			// 临时打开SESSION
			if (retCode==0 && pRemoteSessionImpl == 0)
			{
				tstring sAppModuleName = pcgcParser->getModuleName();
				ModuleItem::pointer moduleItem = m_parseModules.getModuleItem(sAppModuleName);
				if (moduleItem.get() != 0)
				{
					cgcParserSotp::pointer pcgcParserLastSession;
					GetSotpParserPool(pcgcParserLastSession);
					if (pcgcParserLastSession.get() == NULL) return -1;

					//cgcServiceInterface::pointer parserService;
					//m_fpParserSotpService(parserService, cgcNullValueInfo);
					//if (parserService.get() == NULL) return -1;
					//cgcParserSotp::pointer pcgcParserLastSession = CGC_PARSERSOTPSERVICE_DEF(parserService);
					remoteSessionImpl = m_mgrSession.SetSessionImpl(moduleItem,pcgcRemote, pcgcParserLastSession);
					pRemoteSessionImpl = (CSessionImpl*)remoteSessionImpl.get();

					//pRemoteSessionImpl->setAccontInfo(pcgcParser->getAccount(), pcgcParser->getSecure());
					sSessionId = remoteSessionImpl->getId();
					// 调用模块 CGC_Session_Open
					if (pRemoteSessionImpl->OnRunCGC_Session_Open(moduleItem,pcgcRemote))
					{
						bOpenNewSession = true;
						pResponseImpl->SetResponseHandler(pRemoteSessionImpl);
						m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\'.%s opened\n"), sSessionId.c_str(),moduleItem->getName().c_str());
					}else
					{
						// 删除 sessionimpl
						m_mgrSession.RemoveSessionImpl(remoteSessionImpl);
						pRemoteSessionImpl = NULL;
						retCode = -105;
					}
				}
			}
			m_mapRemoteOpenSes.remove(remoteId);
		}

		ModuleItem::pointer pModuleItem;
		if (pRemoteSessionImpl)
		{
			pModuleItem = pRemoteSessionImpl->getModuleItem("",true);	// ?GET DEFAULT
			//printf("**** ProcAppProto pModuleItem=%d\n",(int)pModuleItem.get());
		}
		if (pRemoteSessionImpl == NULL || pModuleItem.get() == NULL)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("invalidate session handle \'%s\'!\n"), sSessionId.c_str());
			retCode = -103;
		}else if (!pModuleItem->getAllowMethod(sCallName, methodName))
		{
			m_logModuleImpl.log(LOG_ERROR, _T("no allow to call \'%s\'!\n"), sCallName.c_str());
			retCode = -106;
		}else
		{
			m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\' call '%s', cid '%d', sign=\"%d\"\n"), sSessionId.c_str(), methodName.c_str(), nCallId, nSign);

			//
			///////////// app call /////////////////
			boost::mutex::scoped_lock * lockWait = NULL;
			CModuleImpl * pModuleImpl = NULL;
			cgcApplication::pointer applicationImpl;
			if (!m_mapOpenModules.find(pModuleItem->getModuleHandle(), applicationImpl))
			{
				m_logModuleImpl.log(LOG_ERROR, _T("invalidate module handle \'%s\'!\n"), pModuleItem->getName().c_str());
				retCode = -102;
			}else if (!applicationImpl->isInited())
			{
				retCode = -105;
			}else
			{
				pModuleImpl = (CModuleImpl*)applicationImpl.get();
				retCode = 0;
				// lock state
				if (pModuleItem->getLockState() == ModuleItem::LS_WAIT)
				{
					// callref
					pModuleImpl->addCallRef();

					// lock
					lockWait = new boost::mutex::scoped_lock(pModuleImpl->m_mutexCallWait);
				}else if (pModuleItem->getLockState() == ModuleItem::LS_RETURN)
				{
					if (pModuleImpl->getCallRef() == 0)
					{
						// callref
						pModuleImpl->addCallRef();
					}else
					{
						m_logModuleImpl.log(LOG_ERROR, _T("module \'%s\' lock return = \'%d\'!\n"), pModuleItem->getName().c_str(), pModuleImpl->getCallRef());
						retCode = -107;
					}
				}else
				{
					// callref
					pModuleImpl->addCallRef();
				}
			}

			if (retCode == 0)
			{
				if (bOpenNewSession)
					pResponseImpl->sendSessionResult(200, sSessionId);

				try
				{
					((CSotpRequestImpl*)requestImpl.get())->setSession(remoteSessionImpl);
					((CSotpResponseImpl*)responseImpl.get())->setSession(remoteSessionImpl);
					retCode = ProcLibMethod(pModuleItem, methodName, requestImpl, responseImpl);
				}catch(std::exception const & e)
				{
					m_logModuleImpl.log(LOG_ERROR, _T("exception, sessionid=\'%s\', callname=\'%s\', lasterror=0x%x\n"), sSessionId.c_str(), methodName.c_str(), GetLastError());
					m_logModuleImpl.log(LOG_ERROR, _T("'%s'\n"), e.what());
					retCode = -111;
				}catch(...)
				{
					m_logModuleImpl.log(LOG_ERROR, _T("exception, sessionid=\'%s\', callname=\'%s\', lasterror=0x%x\n"), sSessionId.c_str(), methodName.c_str(), GetLastError());
					retCode = -111;
				}
				m_logModuleImpl.log(LOG_INFO, _T("SID \'%s\' cid '%d', returnCode '%d'\n"), sSessionId.c_str(), nCallId, retCode);

				// callref
				if (pModuleImpl)
					pModuleImpl->releaseCallRef();
			}

			//
			// unlock
			if (lockWait)
				delete lockWait;
		}
	}else
	{
		m_logModuleImpl.log(LOG_ERROR, _T("invalidate cgc proto type \'%d\'!\n"), pcgcParser->getProtoType());
		retCode = -116;
	}
	if (!pResponseImpl->isResponseSended())
	{
		//responseImpl->lockResponse();	// *
		pResponseImpl->SetResponseHandler(pRemoteSessionImpl);
		pResponseImpl->sendAppCallResult(retCode);
	}
	return 0;
}

HTTP_STATUSCODE CGCApp::ProcHttpLibMethod(const ModuleItem::pointer& moduleItem,const tstring& sMethodName,const cgcHttpRequest::pointer& pRequest,const cgcHttpResponse::pointer& pResponse)
{
	assert (moduleItem.get() != NULL);
	void * hModule = moduleItem->getModuleHandle();
	if (hModule == NULL)
	{
		return STATUS_CODE_404;
	}
#ifdef WIN32
	FPCGCHttpApi farProc = (FPCGCHttpApi)GetProcAddress((HMODULE)hModule, sMethodName.c_str());
#else
	FPCGCHttpApi farProc = (FPCGCHttpApi)dlsym(hModule, sMethodName.c_str());
#endif
	return farProc == NULL ? STATUS_CODE_404 : farProc(pRequest, pResponse);
}

int CGCApp::ProcLibMethod(const ModuleItem::pointer& moduleItem, const tstring & sMethodName, const cgcSotpRequest::pointer& pRequest, const cgcSotpResponse::pointer& pResponse)
{
	assert (moduleItem.get() != NULL);

	int result(0);
	void * hModule = moduleItem->getModuleHandle();
	if (hModule == NULL)
	{
		result = -113;
		return result;
	}
#ifdef WIN32
	//std::string pOutTemp = cgcString::W2Char(sMethodName);
	FPCGCApi farProc = (FPCGCApi)GetProcAddress((HMODULE)hModule, sMethodName.c_str());
#else
	FPCGCApi farProc = (FPCGCApi)dlsym(hModule, sMethodName.c_str());
#endif
	if (farProc == NULL)
	{
		result = -114;
		return result;
	}

	result = farProc(pRequest, pResponse);

	return result;
}

void CGCApp::InitLibModules(unsigned int mt)
{
	//boost::mutex::scoped_lock lock(m_parseModules.m_modules.mutex());

	//CLockMap<void*, cgcApplication::pointer>::iterator iter;
	//for (iter=m_mapOpenModules.begin(); iter!=m_mapOpenModules.end(); iter++)
	//{
		//cgcApplication::pointer application = iter->second;
		//MODULETYPE moduleMT = application->getModuleType();
		//if ((mt & (int)moduleMT) != (int)moduleMT)
		//	continue;

	//CLockMap<tstring, cgc::ModuleItem::pointer,DisableCompare<tstring> >::iterator iter;
	//for (iter=m_parseModules.m_modules.begin(); iter!=m_parseModules.m_modules.end(); iter++)
	for (size_t i=0;i<m_parseModules.m_modules.size();i++)
	{
		ModuleItem::pointer moduleItem = m_parseModules.m_modules[i];
		//ModuleItem::pointer moduleItem = iter->second;
		if (moduleItem->getDisable())
			continue;

		void * hModule = moduleItem->getModuleHandle();
		cgcApplication::pointer application;
		if (!m_mapOpenModules.find(hModule, application)) continue;
		MODULETYPE moduleMT = application->getModuleType();
		if ((mt & (int)moduleMT) != (int)moduleMT)
			continue;

		try
		{
			if (InitLibModule(application, moduleItem))
			{
				CModuleImpl * pModuleImpl = (CModuleImpl*)application.get();
				pModuleImpl->SetInited(true);
				m_logModuleImpl.log(LOG_INFO, _T("MODULE \'%s\' load succeeded\n"), application->getApplicationName().c_str());
			}else
			{
				m_logModuleImpl.log(LOG_ERROR, _T("MODULE \'%s\' load failed\n"), application->getApplicationName().c_str());
			}
#ifdef USES_HDCID
			if (!m_bLicensed)
			{
				m_logModuleImpl.log(DL_WARNING, L"Unlicense account '%s', can only load one module.\n", m_sLicenseAccount.c_str());
				break;
			}
#endif
		}catch(const std::exception & e)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("MODULE \'%s\' load exception.\n"), application->getApplicationName().c_str());
			m_logModuleImpl.log(LOG_ERROR, _T("%s\n"), e.what());
		}catch(...)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("MODULE \'%s\' load exception\n"), application->getApplicationName().c_str());
		}
	}
}

void CGCApp::OpenLibrarys(void)
{
	//CLockMap<tstring, cgc::ModuleItem::pointer,DisableCompare<tstring> >::iterator iter;
	//for (iter=m_parseModules.m_modules.begin(); iter!=m_parseModules.m_modules.end(); iter++)
	for (size_t i=0;i<m_parseModules.m_modules.size(); i++)
	{
		ModuleItem::pointer moduleItem = m_parseModules.m_modules[i];
		//ModuleItem::pointer moduleItem = iter->second;
		if (moduleItem->getDisable())
			continue;

		std::string sTempFile;
		void * hModule = moduleItem->getModuleHandle();
		if (hModule == NULL)
		{
			tstring sModuleName(m_sModulePath);
			sModuleName.append(_T("/modules/"));
			sModuleName.append(moduleItem->getModule());
			
			//m_logModuleImpl.log(LOG_INFO, _T("%s\n"), sModuleName.c_str());

#ifdef WIN32
			hModule = LoadLibrary(sModuleName.c_str());
#else
			hModule = dlopen(sModuleName.c_str(), RTLD_LAZY);
#endif
			if (hModule == NULL)
			{
				sModuleName = moduleItem->getModule();
#ifdef WIN32
				hModule = LoadLibrary(sModuleName.c_str());
				if (hModule == NULL)
				{
					m_logModuleImpl.log(LOG_ERROR, _T("Cannot open library: %s \'%d\'!\n"), sModuleName.c_str(), GetLastError());
					continue;
				}
#else
				hModule = dlopen(sModuleName.c_str(), RTLD_LAZY);
				if (hModule == NULL)
				{
					m_logModuleImpl.log(LOG_ERROR, _T("Cannot open library: \'%s\'!\n"), dlerror());
					continue;
				}
#endif
			}
			if (m_mapOpenModules.exist(hModule))
			{
				// **多次打开，新建临时文件
				namespace fs = boost::filesystem;
				boosttpath pathFileFrom(sModuleName);
				char lpszBuffer[255];
				sprintf(lpszBuffer,"%s.%s",sModuleName.c_str(),moduleItem->getName().c_str());
				sTempFile = lpszBuffer;
				boosttpath pathFileTo(sTempFile);
				boost::filesystem::copy_file(pathFileFrom,pathFileTo,fs::copy_option::overwrite_if_exists);
#ifdef WIN32
				hModule = LoadLibrary(sTempFile.c_str());
#else
				hModule = dlopen(sTempFile.c_str(), RTLD_LAZY);
#endif
				if (hModule == NULL)
				{
					m_logModuleImpl.log(LOG_ERROR, _T("Cannot open library: %s!\n"), sTempFile.c_str());
					boost::filesystem::remove(boosttpath(sTempFile));
					continue;
				}
				m_logModuleImpl.log(LOG_INFO, _T("Open temp file: %s\n"), sTempFile.c_str());
			}
		}
		cgcApplication::pointer application;
		if (m_mapOpenModules.find(hModule, application))
		{
			moduleItem->setModuleHandle(hModule);
			continue;
		}
		CModuleImpl * pModuleImpl = new CModuleImpl(moduleItem);
		pModuleImpl->setModulePath(m_sModulePath);
		pModuleImpl->m_sTempFile = sTempFile;
		cgcApplication::pointer moduleImpl(pModuleImpl);
		m_mapOpenModules.insert(hModule, moduleImpl);

		//
		// CGC_SetApplicationHandler
		FPCGC_SetApplicationHandler fp = 0;
#ifdef WIN32
		fp = (FPCGC_SetApplicationHandler)GetProcAddress((HMODULE)hModule, "CGC_SetApplicationHandler");
#else
		fp = (FPCGC_SetApplicationHandler)dlsym(hModule, "CGC_SetApplicationHandler");
#endif
		if (fp)
			fp(moduleImpl);

		//
		// CGC_SetSystemHandler
		{
			FPCGC_SetSystemHandler fp = 0;
#ifdef WIN32
			fp = (FPCGC_SetSystemHandler)GetProcAddress((HMODULE)hModule, "CGC_SetSystemHandler");
#else
			fp = (FPCGC_SetSystemHandler)dlsym(hModule, "CGC_SetSystemHandler");
#endif
			if (fp)
				fp(shared_from_this());
		}

		// FPCGC_SetServiceManagerHandler
		//if (!pModuleImpl->getModuleItem()->isServiceModule())
		{
#ifdef WIN32
			FPCGC_SetServiceManagerHandler fp = (FPCGC_SetServiceManagerHandler)GetProcAddress((HMODULE)hModule, "CGC_SetServiceManagerHandler");
#else
			FPCGC_SetServiceManagerHandler fp = (FPCGC_SetServiceManagerHandler)dlsym(hModule, "CGC_SetServiceManagerHandler");
#endif
			if (fp)
				fp(shared_from_this());
		}

		moduleItem->setModuleHandle(hModule);
	}
}

void CGCApp::FreeLibrarys(void)
{
	m_mapServiceModule.clear();

	CLockMap<void*, cgcApplication::pointer>::iterator iterApp;
	for (iterApp=m_mapOpenModules.begin(); iterApp!=m_mapOpenModules.end(); iterApp++)
	{
		cgcApplication::pointer application = iterApp->second;
		CModuleImpl * pModuleImpl = (CModuleImpl*)application.get();
		pModuleImpl->StopModule();
	}

	//printf("**** Module Size = %d\n", m_parseModules.m_modules.size());
	//CLockMap<tstring, cgc::ModuleItem::pointer,DisableCompare<tstring> >::iterator iter;
	//for (iter=m_parseModules.m_modules.begin(); iter!=m_parseModules.m_modules.end(); iter++)
	//m_logModuleImpl.log(LOG_INFO, _T("Module Size = %d\n"), m_parseModules.m_modules.size());
	for (size_t i=0;i<m_parseModules.m_modules.size();i++)
	{
		ModuleItem::pointer moduleItem = m_parseModules.m_modules[i];
		//ModuleItem::pointer moduleItem = iter->second;
		if (moduleItem->getDisable())
			continue;

		void * hModule = moduleItem->getModuleHandle();
		printf("**** Free Module(0x%x) %02d -> %s\n", (int)hModule, i+1, moduleItem->getName().c_str());
		//m_logModuleImpl.log(LOG_INFO, _T("Free Module %02d : name=%s, module=0x%x\n"), i+1, moduleItem->getName().c_str(),hModule);
		if (hModule!=NULL)
		{
			try
			{
#ifdef WIN32
				FreeLibrary((HMODULE)hModule);
#else
				dlclose (hModule);
#endif
			}catch(std::exception const & e)
			{
				printf("**** name=%s, exception. 0x%x(%s)\n", moduleItem->getName().c_str(), GetLastError(), e.what());
			}catch(...){
				printf("**** name=%s, exception. 0x%x\n", moduleItem->getName().c_str(), GetLastError());
			}
		}
	}

	// ** delete temp file
	//CLockMap<void*, cgcApplication::pointer>::iterator iterApp;
	for (iterApp=m_mapOpenModules.begin(); iterApp!=m_mapOpenModules.end(); iterApp++)
	{
		cgcApplication::pointer application = iterApp->second;
		CModuleImpl * pModuleImpl = (CModuleImpl*)application.get();
		if (!pModuleImpl->m_sTempFile.empty())
		{
			namespace fs = boost::filesystem;
			boosttpath pathTempFile(pModuleImpl->m_sTempFile);
			boost::filesystem::remove(pathTempFile);
			m_logModuleImpl.log(LOG_INFO, _T("Close temp file: %s\n"), pModuleImpl->m_sTempFile.c_str());
			pModuleImpl->m_sTempFile.clear();
		}
	}
	m_mapOpenModules.clear();
}

bool CGCApp::InitLibModule(const cgcApplication::pointer& moduleImpl, const ModuleItem::pointer& moduleItem)
{
	BOOST_ASSERT(moduleImpl.get() != 0);
	BOOST_ASSERT(moduleItem.get() != 0);

	CModuleImpl * pModuleImpl = (CModuleImpl*)moduleImpl.get();
	//ModuleItem::pointer moduleItem = pModuleImpl->getModuleItem();

	void * hModule = moduleItem->getModuleHandle();
	if (hModule == NULL) return false;

	if (pModuleImpl->getModuleState() == 0)
	{
		pModuleImpl->setModuleState(1);

		// Set log service.
		if (m_fpGetLogService != NULL)
		{
			cgcValueInfo::pointer parameter = CGC_VALUEINFO(pModuleImpl->getModuleItem()->getName());
			cgcServiceInterface::pointer logService;
			m_fpGetLogService(logService, parameter);
			if (logService.get() != NULL)
			{
				logService->initService();
				moduleImpl->logService(CGC_LOGSERVICE_DEF(logService));
			}
		}

		// load allow method
		//	if (!moduleItem->getAllowAll())
		{
			tstring xmlFile(moduleImpl->getAppConfPath());
			xmlFile.append(_T("/methods.xml"));

			namespace fs = boost::filesystem;
			boosttpath pathXmlFile(xmlFile);
			if (boost::filesystem::exists(pathXmlFile))
			{
				XmlParseAllowMethods parseAllowMethods;
				parseAllowMethods.load(xmlFile);
				{
					moduleItem->m_mapAllowMethods = parseAllowMethods.m_mapAllowMethods;
					parseAllowMethods.m_mapAllowMethods.clear();
				}
			}
		}

		//
		// load auths
		//
		if (moduleItem->getAuthAccount())
		{
			tstring xmlFile(moduleImpl->getAppConfPath());
			xmlFile.append(_T("/auths.xml"));

			namespace fs = boost::filesystem;
			boosttpath pathXmlFile(xmlFile);
			if (fs::exists(pathXmlFile))
			{
				XmlParseAuths parseAuths;
				parseAuths.load(xmlFile);
				{
					moduleItem->m_mapAuths = parseAuths.m_mapAuths;
					parseAuths.m_mapAuths.clear();
				}
			}

			if (moduleItem->m_mapAuths.empty())
				m_logModuleImpl.log(LOG_WARNING, _T("'%s' authAccount, please setting '%s'\n"), moduleItem->getModule().c_str(), xmlFile.c_str());
		}

		//
		// init parameter
		tstring xmlFile(moduleImpl->getAppConfPath());
		xmlFile.append(_T("/params.xml"));

		namespace fs = boost::filesystem;
		boosttpath pathXmlFile(xmlFile);
		if (fs::exists(pathXmlFile))
		{
			pModuleImpl->m_moduleParams.clearParameters();
			pModuleImpl->m_moduleParams.load(xmlFile);
		}

		// CGC_Module_Init
#ifdef WIN32
		FPCGC_Module_Init farProc_Init = (FPCGC_Module_Init)GetProcAddress((HMODULE)hModule, "CGC_Module_Init");
#else
		FPCGC_Module_Init farProc_Init = (FPCGC_Module_Init)dlsym(hModule, "CGC_Module_Init");
#endif
		if (farProc_Init)
		{
			bool ret = false;
			try
			{
				ret = farProc_Init();
				static bool theCanRetry = true;
				if (!ret && moduleImpl->getModuleType()==cgc::MODULE_APP && theCanRetry)
				{
					theCanRetry = false;
					// CGC_Module_Free
#ifdef WIN32
					FPCGC_Module_Free farProc_Free = (FPCGC_Module_Free)GetProcAddress((HMODULE)hModule, "CGC_Module_Free");
#else
					FPCGC_Module_Free farProc_Free = (FPCGC_Module_Free)dlsym(hModule, "CGC_Module_Free");
#endif
					const int nMaxTryCount = m_parseDefault.getRetryCount()>10?10:m_parseDefault.getRetryCount();
					for (int i=0;i<nMaxTryCount;i++)
					{
						m_logModuleImpl.log(LOG_INFO, _T("CGC_Module_Init '%s' retry %d...\n"), moduleItem->getName().c_str(),i+1);
						if (farProc_Free)
						{
							farProc_Free();
						}
#ifdef WIN32
						Sleep(3000);
#else
						sleep(3);
#endif
						ret = farProc_Init();
						if (ret || m_bStopedApp)
						{
							break;
						}
					}
				}
			}catch(std::exception const & e)
			{
				m_logModuleImpl.log(LOG_ERROR, _T("%s, 0x%x\n"), e.what(), GetLastError());
			}catch(...){
				m_logModuleImpl.log(LOG_ERROR, _T("0x%x\n"), GetLastError());
			}
			if (!ret)
			{
				m_logModuleImpl.log(LOG_ERROR, _T("CGC_Module_Init '%s' load failed\n"), moduleItem->getName().c_str());
				return false;
			}
		}
	}

#ifdef WIN32
	FPCGC_GetService fpGetService = (FPCGC_GetService)GetProcAddress((HMODULE)hModule, "CGC_GetService");
	FPCGC_ResetService fpResetService = (FPCGC_ResetService)GetProcAddress((HMODULE)hModule, "CGC_ResetService");
#else
	FPCGC_GetService fpGetService = (FPCGC_GetService)dlsym(hModule, "CGC_GetService");
	FPCGC_ResetService fpResetService = (FPCGC_ResetService)dlsym(hModule, "CGC_ResetService");
#endif
	moduleItem->setFpGetService((void*)fpGetService);
	moduleItem->setFpResetService((void*)fpResetService);

	switch (moduleItem->getType())
	{
	case MODULE_LOG:
		{
			if (m_fpGetLogService == NULL)
			{
				m_fpGetLogService = (FPCGC_GetService)moduleItem->getFpGetService();
//#ifdef WIN32
//				m_fpGetLogService = (FPCGC_GetService)GetProcAddress((HMODULE)hModule, "CGC_GetService");
//#else
//				m_fpGetLogService = (FPCGC_GetService)dlsym(hModule, "CGC_GetService");
//#endif
				if (m_fpGetLogService != NULL)
				{
					// LogService owner
					cgcServiceInterface::pointer logService;
					m_fpGetLogService(logService, CGC_VALUEINFO(pModuleImpl->getModuleItem()->getName()));
					moduleImpl->logService(CGC_LOGSERVICE_DEF(logService));

					// MYCP logservice
					logService.reset();
					m_fpGetLogService(logService, CGC_VALUEINFO(m_parseDefault.getCgcpName()));
					m_logModuleImpl.logService(CGC_LOGSERVICE_DEF(logService));
				}
			}

			if (m_fpResetLogService == NULL)
			{
				m_fpResetLogService = (FPCGC_ResetService)moduleItem->getFpResetService();
//#ifdef WIN32
//				m_fpResetLogService = (FPCGC_ResetService)GetProcAddress((HMODULE)hModule, "CGC_ResetService");
//#else
//				m_fpResetLogService = (FPCGC_ResetService)dlsym(hModule, "CGC_ResetService");
//#endif
			}
		}break;
	case MODULE_PARSER:
		{
			if (moduleItem->getProtocol() & PROTOCOL_HTTP)
			{
				// HTTP
				if (m_fpParserHttpService == NULL)
				{
					m_fpParserHttpService = (FPCGC_GetService)moduleItem->getFpGetService();
//#ifdef WIN32
//					m_fpParserHttpService = (FPCGC_GetService)GetProcAddress((HMODULE)hModule, "CGC_GetService");
//#else
//					m_fpParserHttpService = (FPCGC_GetService)dlsym(hModule, "CGC_GetService");
//#endif
				}
			}else if (moduleItem->getProtocol() == PROTOCOL_SOTP)
			{
				// SOTP
				if (m_fpParserSotpService == NULL)
				{
					m_fpParserSotpService = (FPCGC_GetService)moduleItem->getFpGetService();
//#ifdef WIN32
//					m_fpParserSotpService = (FPCGC_GetService)GetProcAddress((HMODULE)hModule, "CGC_GetService");
//#else
//					m_fpParserSotpService = (FPCGC_GetService)dlsym(hModule, "CGC_GetService");
//#endif
				}
			}else
			{
				// ??
			}
		}break;
	case MODULE_COMM:
		{
			if (moduleItem->getCommPort() > 0)
			{
				cgcServiceInterface::pointer serviceInterface = getService(moduleImpl->getApplicationName());
				if (serviceInterface.get() != NULL)
				{
					int nCapacity = atoi(moduleItem->getParam().c_str());
					if (nCapacity < 1)
						nCapacity = 1;
					else
						nCapacity = nCapacity > 100 ? 100 : nCapacity;

					cgcCommunication::pointer commService = CGC_COMMSERVICE_DEF(serviceInterface);
					cgcCommHandler::pointer commHandler = shared_from_this();
					commService->setCommHandler(commHandler);

					//printf("port = %d\n",moduleItem->getCommPort());
					std::vector<cgcValueInfo::pointer> list;
					list.push_back(CGC_VALUEINFO(moduleItem->getCommPort()));
					list.push_back(CGC_VALUEINFO(nCapacity));
					list.push_back(CGC_VALUEINFO(moduleItem->getProtocol()));
					if (!commService->initService(CGC_VALUEINFO(list)))
						this->resetService(serviceInterface);
				}
			}
		}break;
	case MODULE_SERVER:
		{
			if (moduleItem->getProtocol() & PROTOCOL_HTTP)
			{
				tstring functionName = moduleItem->getParam();
				if (functionName.empty())
					functionName = "doHttpServer";

				if (m_fpHttpServer == NULL)
				{
					m_sHttpServerName = moduleItem->getName();
#ifdef WIN32
					m_fpHttpServer = (FPCGCHttpApi)GetProcAddress((HMODULE)hModule, functionName.c_str());
#else
					m_fpHttpServer = (FPCGCHttpApi)dlsym(hModule, functionName.c_str());
#endif
				}
			}
		}break;
	case MODULE_APP:
		{
#ifdef WIN32
			FPCGC_Session_Open fpSessionOpen = (FPCGC_Session_Open)GetProcAddress((HMODULE)hModule, "CGC_Session_Open");
			FPCGC_Session_Close fpSessionClose = (FPCGC_Session_Close)GetProcAddress((HMODULE)hModule, "CGC_Session_Close");
			FPCGC_Remote_Change fpRemoteChange = (FPCGC_Remote_Change)GetProcAddress((HMODULE)hModule, "CGC_Remote_Change");
			FPCGC_Remote_Close fpRemoteClose = (FPCGC_Remote_Close)GetProcAddress((HMODULE)hModule, "CGC_Remote_Close");
#else
			FPCGC_Session_Open fpSessionOpen = (FPCGC_Session_Open)dlsym(hModule, "CGC_Session_Open");
			FPCGC_Session_Close fpSessionClose = (FPCGC_Session_Close)dlsym(hModule, "CGC_Session_Close");
			FPCGC_Remote_Change fpRemoteChange = (FPCGC_Remote_Change)dlsym(hModule, "CGC_Remote_Change");
			FPCGC_Remote_Close fpRemoteClose = (FPCGC_Remote_Close)dlsym(hModule, "CGC_Remote_Close");
#endif
			moduleItem->setFpSessionOpen((void*)fpSessionOpen);
			moduleItem->setFpSessionClose((void*)fpSessionClose);
			moduleItem->setFpRemoteChange((void*)fpRemoteChange);
			moduleItem->setFpRemoteClose((void*)fpRemoteClose);
		}break;
	default:
		break;
	}

	return true;
}

void CGCApp::FreeLibModules(unsigned int mt)
{
	//boost::mutex::scoped_lock lock(m_parseModules.m_modules.mutex());

	// 反向遍历
	CLockMap<void*, cgcApplication::pointer>::reverse_iterator iter;
	for (iter=m_mapOpenModules.rbegin(); iter!=m_mapOpenModules.rend(); iter++)
	//CLockMap<void*, cgcApplication::pointer>::iterator iter;
	//for (iter=m_mapOpenModules.begin(); iter!=m_mapOpenModules.end(); iter++)
	{
		cgcApplication::pointer application = iter->second;
		MODULETYPE moduleMT = application->getModuleType();
		if ((mt & (int)moduleMT) != (int)moduleMT)
			continue;

		try
		{
			FreeLibModule(application);
			m_logModuleImpl.log(LOG_INFO, _T("MODULE \'%s\' free succeeded\n"), application->getApplicationName().c_str());
		}catch(const std::exception & e)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("MODULE \'%s\' free exception.\n"), application->getApplicationName().c_str());
			m_logModuleImpl.log(LOG_ERROR, _T("%s\n"), e.what());
		}catch(...)
		{
			m_logModuleImpl.log(LOG_ERROR, _T("MODULE \'%s\' free exception\n"), application->getApplicationName().c_str());
		}
	}
}

void CGCApp::FreeLibModule(const cgcApplication::pointer& moduleImpl)
{
	if (moduleImpl.get() == NULL) return;

	CModuleImpl * pModuleImpl = (CModuleImpl*)moduleImpl.get();
	ModuleItem::pointer moduleItem = pModuleImpl->getModuleItem();
	if (moduleItem.get() == 0) return;

	void * hModule = moduleItem->getModuleHandle();
	if (hModule != NULL)
	{
		CLockMap<cgcServiceInterface::pointer, void*>::iterator iterService;
		for (iterService=m_mapServiceModule.begin(); iterService!=m_mapServiceModule.end(); )
		{
			if (iterService->second == hModule)
			{
				this->resetService(iterService->first);
				if (m_mapServiceModule.empty())
					break;
				iterService=m_mapServiceModule.begin();
			}else
			{
				iterService++;
			}
		}

		if (moduleImpl->getModuleType() == MODULE_LOG)
		{
			if (m_fpResetLogService != NULL && moduleImpl->logService().get() != NULL)
			{
				m_fpResetLogService(moduleImpl->logService());
				moduleImpl->logService(cgcNullLogService);
			}
		}

		if (pModuleImpl->getModuleState() != -1)
		{
			pModuleImpl->setModuleState(-1);
			
			// CGC_Module_Free
			FPCGC_Module_Free farProc_Free = 0;
#ifdef WIN32
			farProc_Free = (FPCGC_Module_Free)GetProcAddress((HMODULE)hModule, "CGC_Module_Free");
#else
			farProc_Free = (FPCGC_Module_Free)dlsym(hModule, "CGC_Module_Free");
#endif
			if (farProc_Free)
			{
				//m_logModuleImpl.log(LOG_DEBUG, _T("CGC_Module_Free '%s'\n"), moduleItem->getAppName().c_str());
				try{
					farProc_Free();
				}catch(std::exception const & e)
				{
					m_logModuleImpl.log(LOG_ERROR, _T("%s, 0x%x\n"), e.what(), GetLastError());
				}catch(...){
					m_logModuleImpl.log(LOG_ERROR, _T("0x%x\n"), GetLastError());
				}
			}
		}
	}

	if (m_fpResetLogService != NULL && moduleImpl->logService().get() != NULL)
		m_fpResetLogService(moduleImpl->logService());

	moduleImpl->KillAllTimer();
	cgcAttributes::pointer attributes = moduleImpl->getAttributes();
	if (attributes.get() != NULL)
	{
		attributes->clearAllAtrributes();
		attributes->cleanAllPropertys();
	}
}
