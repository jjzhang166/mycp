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

// CGCApp.h file here
#ifndef _CGCApp_HEAD_VER_1_0_0_0__INCLUDED_
#define _CGCApp_HEAD_VER_1_0_0_0__INCLUDED_

#define CGC_IMPORTS 1

#include "IncludeBase.h"
#include <CGCBase/cgcSeqInfo.h>

//#include "ACELib/inc/SockServer.h"

#include <CGCClass/AttributesImpl.h>
//#include "AttributesImpl.h"
#include "XmlParseDefault.h"
#include "XmlParseModules.h"
#include "XmlParseParams.h"
#include "XmlParseCdbcs.h"
#include "XmlParseAllowMethods.h"
#include "XmlParseAuths.h"
#include "XmlParseWeb.h"
//#include "XmlParseClusters.h"
#include "ModuleMgr.h"
#include "SessionMgr.h"
#include "DataServiceMgr.h"
#include "SotpRequestImpl.h"
#include "SotpResponseImpl.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "ResponseImpl.h"

class CMySessionInfo
{
public:
	typedef boost::shared_ptr<CMySessionInfo> pointer;
	static CMySessionInfo::pointer create(const tstring & sMySessionId,const tstring& sUserAgent)
	{
		return CMySessionInfo::pointer(new CMySessionInfo(sMySessionId,sUserAgent));
	}

	CMySessionInfo(void)
		: m_tRequestTime(0)
	{
	}
	CMySessionInfo(const tstring& sMySessionId,const tstring& sUserAgent)
		: m_sMySessionId(sMySessionId)
		, m_sUserAgent(sUserAgent)
	{
		m_tRequestTime = time(0);
	}
	const tstring& GetMySessionId(void) const {return m_sMySessionId;}
	const tstring& GetUserAgent(void) const {return m_sUserAgent;}
	const time_t GetRequestTime(void) const {return m_tRequestTime;}
	void UpdateRequestTime(void) {m_tRequestTime = time(0);}
private:
	tstring m_sMySessionId;
	tstring m_sUserAgent;
	time_t m_tRequestTime;
};

class CGCApp
	: public cgcSystem
	, public cgcServiceManager
	, public cgcCommHandler
	, public boost::enable_shared_from_this<CGCApp>
{
public:
	typedef boost::shared_ptr<CGCApp> pointer;

	static CGCApp::pointer create(const tstring & sPath)
	{
		return CGCApp::pointer(new CGCApp(sPath));
	}

	CGCApp(const tstring & sPath);
	virtual ~CGCApp(void);

public:
	int MyMain(bool bService = false);

	bool isInited(void) const {return m_bInitedApp;}
	bool isExitLog(void) const {return m_bExitLog;}
	bool ProcLastAccessedTime(void);
	bool ProcDataResend(void) {return this->m_mgrSession.ProcDataResend();}

	void AppInit(bool bNTService = true);
	void AppStart(void);
	void AppStop(void);
	void AppExit(void);
	void PrintHelp(void);

	// cgcServiceManager handler
	virtual cgcServiceInterface::pointer getService(const tstring & serviceName, const cgcValueInfo::pointer& parameter = cgcNullValueInfo);
	virtual void resetService(const cgcServiceInterface::pointer & service);
private:
	void LoadDefaultConf(void);
	void LoadClustersConf(void);
	void LoadAuthsConf(void);
	void LoadModulesConf(void);
	void LoadSystemParams(void);

	// cgcCommHandler handler
	virtual int onRemoteAccept(const cgcRemote::pointer& pcgcRemote);
	virtual int onRecvData(const cgcRemote::pointer& pcgcRemote, const unsigned char * recvData, size_t recvLen);
	virtual int onRemoteClose(unsigned long remoteId, int nErrorCode);

	// cgcServiceManager handler
	virtual cgcCDBCService::pointer getCDBDService(const tstring& datasource);
	virtual void retCDBDService(cgcCDBCServicePointer& cdbcservice);
	virtual HTTP_STATUSCODE executeInclude(const tstring & url, const cgcHttpRequest::pointer & request, const cgcHttpResponse::pointer& response);
	virtual HTTP_STATUSCODE executeService(const tstring & serviceName, const tstring& function, const cgcHttpRequest::pointer & request, const cgcHttpResponse::pointer& response, tstring & outExecuteResult);

	// cgcSystem handler
	virtual cgcParameterMap::pointer getInitParameters(void) const {return m_systemParams.getParameters();}
	//virtual cgcCDBCInfo::pointer getCDBCInfo(const tstring& name) const {return m_cdbcs.getCDBCInfo(name);}

	virtual const tstring & getServerPath(void) const {return m_sModulePath;}
	virtual const tstring & getServerName(void) const {return m_parseDefault.getCgcpName();}
	virtual const tstring & getServerAddr(void) const {return m_parseDefault.getCgcpAddr();}
	virtual const tstring & getServerCode(void) const {return m_parseDefault.getCgcpCode();}
	virtual int getServerRank(void) const {return m_parseDefault.getCgcpRank();}
	virtual cgcSession::pointer getSession(const tstring & sessionId) const {return m_mgrSession.GetSessionImpl(sessionId);}
	virtual cgcResponse::pointer getLastResponse(const tstring & sessionId,const tstring& moduleName) const;
	virtual cgcResponse::pointer getHoldResponse(const tstring& sessionId,unsigned long remoteId);

	virtual cgcAttributes::pointer getAttributes(bool create);
	virtual cgcAttributes::pointer getAttributes(void) const {return m_attributes;}
	virtual cgcAttributes::pointer getAppAttributes(const tstring & appName) const;

	tstring SetNewMySessionId(cgcParserHttp::pointer& phttpParser,const tstring& sSessionId="");
	HTTP_STATUSCODE ProcHttpData(const unsigned char * recvData, size_t dataSize,const cgcRemote::pointer& pcgcRemote);
	HTTP_STATUSCODE ProcHttpAppProto(const cgcHttpRequest::pointer& pRequestImpl,const cgcHttpResponse::pointer& pResponseImpl,const cgcParserHttp::pointer& pcgcParser);
	HTTP_STATUSCODE ProcHttpLibMethod(const ModuleItem::pointer& moduleItem,const tstring& sMethodName,const cgcHttpRequest::pointer& pRequest,const cgcHttpResponse::pointer& pResponse);
	int ProcCgcData(const unsigned char * recvData, size_t dataSize, const cgcRemote::pointer& pcgcRemote);
	// pRemoteSessionImpl 可以为空；
	int ProcSesProto(const cgcSotpRequest::pointer& pRequestImpl, const cgcParserSotp::pointer& pcgcParser, const cgcRemote::pointer& pcgcRemote, cgcSession::pointer& pRemoteSessionImpl);
	int ProcAppProto(const cgcSotpRequest::pointer& pRequestImpl, const cgcSotpResponse::pointer& pResponseImpl, const cgcParserSotp::pointer& pcgcParser, cgcSession::pointer& pRemoteSessionImpl);
	int ProcLibMethod(const ModuleItem::pointer& moduleItem, const tstring & sMethodName, const cgcSotpRequest::pointer& pRequest, const cgcSotpResponse::pointer& pResponse);

	void OpenLibrarys(void);
	void FreeLibrarys(void);
	void InitLibModules(unsigned int mt);
	bool InitLibModule(const cgcApplication::pointer& pModuleImpl, const ModuleItem::pointer& moduleItem);
	void FreeLibModules(unsigned int mt);
	void FreeLibModule(const cgcApplication::pointer& pModuleImpl);

private:
	XmlParseDefault m_parseDefault;
	XmlParseModules m_parseModules;
	XmlParsePortApps m_parsePortApps;
	XmlParseWeb m_parseWeb;
//	XmlParseClusters m_parseClusters;
//	XmlParseAuths m_parseAuths;

	XmlParseParams m_systemParams;
	XmlParseCdbcs m_cdbcs;
	CDataServiceMgr m_cdbcServices;
	cgcAttributes::pointer m_attributes;

	CModuleImpl m_logModuleImpl;
	CSessionMgr m_mgrSession;
	//CHttpSessionMgr m_mgrHttpSession;

	tstring m_sModulePath;
	boost::thread * m_pProcSessionTimeout;
	boost::thread * m_pProcDataResend;
	//CLockMap<short, cgcSeqInfo::pointer> m_mapSeqInfo;

	bool m_bInitedApp;
	bool m_bStopedApp;
	bool m_bExitLog;

	bool m_bLicensed;				// default true
	tstring m_sLicenseAccount;
	int m_licenseModuleCount;

	FPCGC_GetService m_fpGetLogService;
	FPCGC_ResetService m_fpResetLogService;
	FPCGC_GetService m_fpParserSotpService;
	FPCGC_GetService m_fpParserHttpService;
	//FPCGCHttpApi m_fpHttpStruct;
	FPCGCHttpApi m_fpHttpServer;
	tstring m_sHttpServerName;

	CLockMap<unsigned long, bool> m_mapRemoteOpenSes;		// remoteid->boost::mutex
	CLockMap<cgcServiceInterface::pointer, void*> m_mapServiceModule;		// cgcService->MODULE HANDLE

	CLockMap<unsigned long, cgcMultiPart::pointer> m_mapMultiParts;		// remoteid->
	CLockMap<void*, cgcApplication::pointer> m_mapOpenModules;			//
	//CLockMap<tstring,CMySessionInfo::pointer> m_pMySessionInfoList;		// mysessionid-> (后期考虑保存到硬盘)
};

#endif // _CGCApp_HEAD_VER_1_0_0_0__INCLUDED_
