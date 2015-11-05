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
#pragma warning(disable:4267 4996)
#include <windows.h>
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
    return TRUE;
}
#endif

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

// cgc head
#include <CGCBase/http.h>
#include <CGCBase/cgcServices.h>
#include <CGCBase/cgcCDBCService.h>
using namespace cgc;
#include "MycpHttpServer.h"
#include "XmlParseHosts.h"
#include "XmlParseApps.h"
#include "XmlParseDSs.h"
#include "md5.h"

//#define USES_BODB_SCRIPT

#ifdef USES_BODB_SCRIPT
cgcCDBCService::pointer theCdbcService;
#endif
bool theEnableDataSource = false;
cgcServiceInterface::pointer theFileSystemService;
//cgcServiceInterface::pointer theStringService;
XmlParseHosts theVirtualHosts;
XmlParseApps::pointer theApps;
XmlParseDSs::pointer theDataSources;
CVirtualHost::pointer theDefaultHost;
CLockMap<tstring, CFileScripts::pointer> theFileScripts;
CLockMap<tstring, CCSPFileInfo::pointer> theFileInfos;
CLockMap<tstring, CCSPFileInfo::pointer> theIdleFileInfos;	// ��¼�����ļ�������7���޷��ʣ�ɾ�����ݿ����ݣ�
cgcAttributes::pointer theAppAttributes;
const int ATTRIBUTE_FILE_INFO = 8;				// filepath->
const unsigned int TIMERID_1_SECOND = 1;		// 1����һ��

class CResInfo : public cgcObject
{
public:
	typedef boost::shared_ptr<CResInfo> pointer;
	static CResInfo::pointer create(const std::string& sFileName, const std::string& sMimeType)
	{
		return CResInfo::pointer(new CResInfo(sFileName,sMimeType));
	}
	CResInfo(const std::string& sFileName, const std::string& sMimeType)
		: m_sFileName(sFileName),m_sMimeType(sMimeType)
		, m_tModifyTime(0),m_tRequestTime(0)
		, m_nSize(0),m_pData(NULL)
	{
	}
	CResInfo(void)
		: m_tModifyTime(0),m_tRequestTime(0)
		, m_nSize(0),m_pData(NULL)
	{
	}
	virtual ~CResInfo(void)
	{
		if (m_pData!=NULL)
		{
			delete[] m_pData;
			m_pData = NULL;
		}
		//m_fs.close();
	}
	boost::mutex m_mutex;
	std::string m_sFileName;
	std::string m_sMimeType;
	//std::fstream m_fs;
	time_t m_tModifyTime;
	std::string m_sModifyTime;
	std::string m_sETag;
	time_t m_tRequestTime;
	unsigned int m_nSize;
	char * m_pData;
};

class CHttpTimeHandler
	: public cgcOnTimerHandler
{
public:
	typedef boost::shared_ptr<CHttpTimeHandler> pointer;
	static CHttpTimeHandler::pointer create(void)
	{
		return CHttpTimeHandler::pointer(new CHttpTimeHandler());
	}
	CHttpTimeHandler(void)
	{}

	virtual void OnTimeout(unsigned int nIDEvent, const void * pvParam)
	{
		if (nIDEvent==TIMERID_1_SECOND)
		{
			static unsigned int theSecondIndex = 0;
			theSecondIndex++;

			if ((theSecondIndex%(24*3600))==23*3600)	// һ�촦��һ��
			{
				std::vector<tstring> pRemoveFileNameList;
				if (!theIdleFileInfos.empty())
				{
					// ɾ������10��û���ļ����ݣ�
					const time_t tNow = time(0);
#ifdef USES_BODB_SCRIPT
					char sql[512];
#endif
					BoostReadLock rdlock(theIdleFileInfos.mutex());
					CLockMap<tstring, CCSPFileInfo::pointer>::iterator pIter = theIdleFileInfos.begin();
					for (; pIter!=theIdleFileInfos.end(); pIter++)
					{
						CCSPFileInfo::pointer fileInfo = pIter->second;
						if (fileInfo->m_tRequestTime==0 || (fileInfo->m_tRequestTime+(10*24*3600))<tNow)	// ** 10 days
						{
							const tstring sFileName(pIter->first);
							pRemoveFileNameList.push_back(sFileName);
							theFileInfos.remove(sFileName);
							theFileScripts.remove(sFileName);
#ifdef USES_BODB_SCRIPT
							if (theCdbcService.get()!=NULL)
							{
								tstring sFileNameTemp(sFileName);
								theCdbcService->escape_string_in(sFileNameTemp);
								sprintf(sql, "DELETE FROM scriptitem_t WHERE filename='%s'", sFileNameTemp.c_str());
								theCdbcService->execute(sql);
								sprintf(sql, "DELETE FROM cspfileinfo_t WHERE filename='%s')",sFileNameTemp.c_str());
								theCdbcService->execute(sql);
							}
#endif
						}
					}
				}
				for (size_t i=0; i<pRemoveFileNameList.size(); i++)
				{
					theIdleFileInfos.remove(pRemoveFileNameList[i]);
				}
				pRemoveFileNameList.clear();

			}
			if ((theSecondIndex%3600)==3500)	// 3600=60���Ӵ���һ��
			{
				const time_t tNow = time(0);

				std::vector<tstring> pRemoveFileNameList;
				if (!theFileInfos.empty())
				{
					// ����2Сʱû�з����ļ��� �ŵ������б�
					BoostReadLock rdlock(theFileInfos.mutex());
					CLockMap<tstring, CCSPFileInfo::pointer>::iterator pIter = theFileInfos.begin();
					for (; pIter!=theFileInfos.end(); pIter++)
					{
						CCSPFileInfo::pointer fileInfo = pIter->second;
						if (fileInfo->m_tRequestTime>0 && (fileInfo->m_tRequestTime+(3*3600))<tNow)	// ** 3 hours
						{
							if (!theFileScripts.remove(pIter->first))
							{
								pRemoveFileNameList.push_back(pIter->first);
								theIdleFileInfos.insert(pIter->first,pIter->second,false);
							}
						}
					}
				}
				for (size_t i=0; i<pRemoveFileNameList.size(); i++)
				{
					theFileInfos.remove(pRemoveFileNameList[i]);
				}
				pRemoveFileNameList.clear();

				StringObjectMapPointer pFileInfoList = theAppAttributes->getStringAttributes(ATTRIBUTE_FILE_INFO,false);
				if (pFileInfoList.get() != NULL && !pFileInfoList->empty())
				{
					std::vector<tstring> pRemoveFilePathList;
					{
						BoostReadLock rdlock(pFileInfoList->mutex());
						CObjectMap<tstring>::const_iterator pIter = pFileInfoList->begin();
						for (;pIter!=pFileInfoList->end();pIter++)
						{
							const CResInfo::pointer pResInfo = CGC_OBJECT_CAST<CResInfo>(pIter->second);
							if (pResInfo->m_tRequestTime>0 && (pResInfo->m_tRequestTime+(3*3600))<tNow)	// ** 3 hours
							{
								pRemoveFilePathList.push_back(pIter->first);
							}
						}
					}
					for (size_t i=0; i<pRemoveFilePathList.size(); i++)
					{
						pFileInfoList->remove(pRemoveFilePathList[i]);
						//theAppAttributes->removeAttribute(ATTRIBUTE_FILE_INFO, pRemoveFilePathList[i]);
					}
					pRemoveFilePathList.clear();
				}
			}

		}
	}
};
CHttpTimeHandler::pointer theTimerHandler;

extern "C" bool CGC_API CGC_Module_Init(void)
{
	theFileSystemService = theServiceManager->getService("FileSystemService");
	if (theFileSystemService.get() == NULL)
	{
		CGC_LOG((cgc::LOG_ERROR, "FileSystemService Error.\n"));
		return false;
	}
	//theStringService = theServiceManager->getService("StringService");
	//if (theStringService.get() == NULL)
	//{
	//	CGC_LOG((cgc::LOG_ERROR, "StringService Error.\n"));
	//	return false;
	//}

	cgcParameterMap::pointer initParameters = theApplication->getInitParameters();

	// Load APP calls.
	tstring xmlFile(theApplication->getAppConfPath());
	xmlFile.append("/apps.xml");
	cgcValueInfo::pointer outProperty = CGC_VALUEINFO(false);
	theFileSystemService->callService("exists", CGC_VALUEINFO(xmlFile), outProperty);
	if (outProperty->getBoolean())
	{
		theApps = XMLPARSEAPPS;
		theApps->load(xmlFile);
	}

	// Load DataSource.
	xmlFile = theApplication->getAppConfPath();
	xmlFile.append("/datasources.xml");
	outProperty->setBoolean(false);
	theFileSystemService->callService("exists", CGC_VALUEINFO(xmlFile), outProperty);
	if (outProperty->getBoolean())
	{
		theDataSources = XMLPARSEDSS;
		theDataSources->load(xmlFile);
	}

	// Load virtual hosts.
	xmlFile = theApplication->getAppConfPath();
	xmlFile.append("/hosts.xml");
	theVirtualHosts.load(xmlFile);

	theDefaultHost = theVirtualHosts.getVirtualHost("*");
	if (theDefaultHost.get() == NULL)
	{
		CGC_LOG((cgc::LOG_ERROR, "DefaultHost Error. %s\n",xmlFile.c_str()));
		return false;
	}
	CGC_LOG((cgc::LOG_INFO, "%s\n",theDefaultHost->getDocumentRoot().c_str()));
	theDefaultHost->setPropertys(theApplication->createAttributes());
	if (theDefaultHost->getDocumentRoot().empty())
		theDefaultHost->setDocumentRoot(theApplication->getAppConfPath());
	else
	{
		tstring sDocumentRoot = theDefaultHost->getDocumentRoot();
		namespace fs = boost::filesystem;
		fs::path src_path(sDocumentRoot);
		if (!fs::exists(src_path))
		{
			sDocumentRoot = theApplication->getAppConfPath();
			sDocumentRoot.append("/");
			sDocumentRoot.append(theDefaultHost->getDocumentRoot());
			fs::path src_path2(sDocumentRoot);
			if (!fs::exists(src_path2))
			{
				CGC_LOG((cgc::LOG_ERROR, "DocumentRoot not exist. %s\n",sDocumentRoot.c_str()));
				return false;
			}

			theDefaultHost->setDocumentRoot(sDocumentRoot);
		}
	}
	theDefaultHost->m_bBuildDocumentRoot = true;

	theEnableDataSource = initParameters->getParameterValue("enable-datasource", 0)==1?true:false;
#ifdef USES_BODB_SCRIPT
	if (theEnableDataSource)
	{
		tstring cdbcDataSource = initParameters->getParameterValue("CDBCDataSource", "ds_httpserver");
		theCdbcService = theServiceManager->getCDBDService(cdbcDataSource);
		if (theCdbcService.get() == NULL)
		{
			CGC_LOG((cgc::LOG_ERROR, "HttpServer DataSource Error.\n"));
			return false;
		}
		// 
		char selectSql[512];
		int cdbcCookie = 0;
		theCdbcService->select("SELECT filename,filesize,lasttime FROM cspfileinfo_t", cdbcCookie);
		//printf("**** ret=%d,cookie=%d\n",ret,cdbcCookie);
		cgcValueInfo::pointer record = theCdbcService->first(cdbcCookie);
		while (record.get() != NULL)
		{
			//assert (record->getType() == cgcValueInfo::TYPE_VECTOR);
			//assert (record->size() == 3);

			tstring sfilename(record->getVector()[0]->getStr());
			sprintf(selectSql, "SELECT code FROM scriptitem_t WHERE filename='%s' AND length(code)=3 LIMIT 1", sfilename.c_str());
			if (theCdbcService->select(selectSql)==0)
			{
				theCdbcService->escape_string_out(sfilename);
				cgcValueInfo::pointer var_filesize = record->getVector()[1];
				cgcValueInfo::pointer var_lasttime = record->getVector()[2];
				CCSPFileInfo::pointer fileInfo = CSP_FILEINFO(sfilename, var_filesize->getIntValue(), var_lasttime->getBigIntValue());
				theFileInfos.insert(fileInfo->getFileName(), fileInfo, false);
			}else
			{
				sprintf(selectSql, "DELETE FROM scriptitem_t WHERE filename='%s'", sfilename.c_str());
				theCdbcService->execute(selectSql);
				sprintf(selectSql, "DELETE FROM cspfileinfo_t WHERE filename='%s')",sfilename.c_str());
				theCdbcService->execute(selectSql);
			}

			record = theCdbcService->next(cdbcCookie);
		}
		theCdbcService->reset(cdbcCookie);
	}
#endif

	theAppAttributes = theApplication->getAttributes(true);
	theTimerHandler = CHttpTimeHandler::create();
	theApplication->SetTimer(TIMERID_1_SECOND, 1*1000, theTimerHandler);	// 1����һ��

	return true;
}

extern "C" void CGC_API CGC_Module_Free(void)
{
	cgcAttributes::pointer attributes = theApplication->getAttributes();
	if (attributes.get() != NULL)
	{
		// C++ APP
		std::vector<cgcValueInfo::pointer> apps;
		attributes->getProperty((int)OBJECT_APP, apps);
		for (size_t i=0; i<apps.size(); i++)
		{
			cgcValueInfo::pointer var = apps[i];
			if (var->getType() == cgcValueInfo::TYPE_OBJECT && var->getInt() == (int)OBJECT_APP)
			{
				theServiceManager->resetService(CGC_OBJECT_CAST<cgcServiceInterface>(var->getObject()));
			}
		}

		// C++ CDBC
		std::vector<cgcValueInfo::pointer> cdbcs;
		attributes->getProperty((int)OBJECT_CDBC, cdbcs);
		for (size_t i=0; i<cdbcs.size(); i++)
		{
			cgcValueInfo::pointer var = cdbcs[i];
			if (var->getType() == cgcValueInfo::TYPE_OBJECT && var->getInt() == (int)OBJECT_CDBC)
			{
				theServiceManager->resetService(CGC_OBJECT_CAST<cgcServiceInterface>(var->getObject()));
			}

		}

		attributes->cleanAllPropertys();
	}
	theApplication->KillAllTimer();
	if (theAppAttributes.get()!=NULL)
	{
		theAppAttributes->clearAllAtrributes();
		theAppAttributes.reset();
	}

#ifdef USES_BODB_SCRIPT
	theServiceManager->retCDBDService(theCdbcService);
#endif
	theFileSystemService.reset();
	//theStringService.reset();
	theDefaultHost.reset();
	theVirtualHosts.clear();
	theApps.reset();
	theDataSources.reset();
	theFileScripts.clear();
	theFileInfos.clear();
	theIdleFileInfos.clear();
	theTimerHandler.reset();
}

/*
application/postscript                *.ai *.eps *.ps         Adobe Postscript-Dateien 
application/x-httpd-php             *.php *.phtml         PHP-Dateien 
audio/basic                             *.au *.snd             Sound-Dateien 
audio/x-midi                            *.mid *.midi             MIDI-Dateien 
audio/x-mpeg                         *.mp2             MPEG-Dateien 
image/x-windowdump             *.xwd             X-Windows Dump 
video/mpeg                          *.mpeg *.mpg *.mpe         MPEG-Dateien 
video/vnd.rn-realvideo            *.rmvb              realplay-Dateien 
video/quicktime                     *.qt *.mov             Quicktime-Dateien 
video/vnd.vivo                       *viv *.vivo             Vivo-Dateien
*/

bool isCSPFile(const tstring& filename, tstring& outMimeType,bool& pOutImageOrBinary)
{
	outMimeType = "text/html";
	tstring::size_type find = filename.rfind(".");
	if (find == tstring::npos)
	{
		outMimeType = "application/octet-stream";
		return false;
	}

	pOutImageOrBinary = true;
	tstring ext = filename.substr(find+1);
	if (ext == "csp")
	{
		pOutImageOrBinary = false;
		return true;
	}else if (ext == "gif")
		outMimeType = "image/gif";
	else if(ext == "jpeg" || ext == "jpg" || ext == "jpe")
		outMimeType = "image/jpeg";
	else if(ext == "htm" || ext == "html" || ext == "shtml")
	{
		pOutImageOrBinary = false;
		outMimeType = "text/html";
	}else if(ext == "js")
	{
		pOutImageOrBinary = false;
		outMimeType = "text/javascript";
	}else if(ext == "css")
	{
		pOutImageOrBinary = false;
		outMimeType = "text/css";
	}else if(ext == "txt")
	{
		pOutImageOrBinary = false;
		outMimeType = "text/plain";
	}else if(ext == "xls" || ext == "xla")
		outMimeType = "application/msexcel";
	else if(ext == "hlp" || ext == "chm")
		outMimeType = "application/mshelp";
	else if(ext == "ppt" || ext == "ppz" || ext == "pps" || ext == "pot")
		outMimeType = "application/mspowerpoint";
	else if(ext == "doc" || ext == "dot")
		outMimeType = "application/msword";
	else if(ext == "exe")
		outMimeType = "application/octet-stream";
	else if(ext == "pdf")
		outMimeType = "application/pdf";
	else if(ext == "rtf")
		outMimeType = "application/rtf";
	else if(ext == "zip")
		outMimeType = "application/zip";
	else if(ext == "jar")
		outMimeType = "application/java-archive";
	else if(ext == "swf" || ext == "cab")
		outMimeType = "application//x-shockwave-flash";
	else if(ext == "mp3")
		outMimeType = "audio/mpeg";
	else if(ext == "wav")
		outMimeType = "audio/x-wav";
	else
		pOutImageOrBinary = false;

	return false;
}
#ifdef USES_BODB_SCRIPT
void insertScriptItem(const tstring& code, const tstring & sFileNameTemp, const CScriptItem::pointer& scriptItem, int sub = 0)
{
	std::string sValue(scriptItem->getValue());
	theCdbcService->escape_string_in(sValue);
	const int sqlsize = sValue.size()+1000;
	char * sql = new char[sqlsize];

	sprintf(sql, "INSERT INTO scriptitem_t (filename,code,sub,itemtype,object1,object2,id,scopy,name,property,type,value) VALUES('%s','%s',%d,%d,%d,%d,'%s','%s','%s','%s','%s','%s')",
				 sFileNameTemp.c_str(), code.c_str(), sub,(int)scriptItem->getItemType(),(int)scriptItem->getOperateObject1(),(int)scriptItem->getOperateObject2(),
				 scriptItem->getId().c_str(), scriptItem->getScope().c_str(),scriptItem->getName().c_str(), scriptItem->getProperty().c_str(),scriptItem->getType().c_str(), sValue.c_str());
	theCdbcService->execute(sql);
	delete[] sql;

	char bufferCode[64];
	for (size_t i=0; i<scriptItem->m_subs.size(); i++)
	{
		CScriptItem::pointer subScriptItem = scriptItem->m_subs[i];
		sprintf(bufferCode, "%s1%03d", code.c_str(), i);
		insertScriptItem(bufferCode, sFileNameTemp, subScriptItem, 1);
	}
	for (size_t i=0; i<scriptItem->m_elseif.size(); i++)
	{
		CScriptItem::pointer subScriptItem = scriptItem->m_elseif[i];
		sprintf(bufferCode, "%s2%03d", code.c_str(), i);
		insertScriptItem(bufferCode, sFileNameTemp, subScriptItem, 2);
	}
	for (size_t i=0; i<scriptItem->m_else.size(); i++)
	{
		CScriptItem::pointer subScriptItem = scriptItem->m_else[i];
		sprintf(bufferCode, "%s3%03d", code.c_str(), i);
		insertScriptItem(bufferCode, sFileNameTemp, subScriptItem, 3);
	}
}
#endif

const int const_one_hour_seconds = 60*60;
const int const_one_day_seconds = 24*const_one_hour_seconds;
const int const_memory_size = 50*1024*1024;		// max 50MB
const int const_max_size = 35*1024*1024;		// max 35MB	// 30

void SetExpiresCache(const cgcHttpResponse::pointer& response,time_t tRequestTime, bool bIsImageOrBinary)
{
	const int nExpireSecond = bIsImageOrBinary?(10*const_one_day_seconds):(2*const_one_day_seconds);	// �ı��ļ���2�죻����ͼƬ��10�죻
	const time_t tExpiresTime = tRequestTime + nExpireSecond;
	struct tm * tmExpiresTime = gmtime(&tExpiresTime);
	char lpszBuffer[64];
	strftime(lpszBuffer, 64, "%a, %d %b %Y %H:%M:%S GMT", tmExpiresTime);	// Tue, 19 Aug 2015 07:00:19 GMT
	response->setHeader("Expires",lpszBuffer);
	sprintf(lpszBuffer, "max-age=%d", nExpireSecond);
	response->setHeader("Cache-Control",lpszBuffer);
}

extern "C" HTTP_STATUSCODE CGC_API doHttpServer(const cgcHttpRequest::pointer & request, const cgcHttpResponse::pointer& response)
{
	HTTP_STATUSCODE statusCode = STATUS_CODE_200;
	const tstring sIfModifiedSince(request->getHeader("if-modified-since"));	// for Last-Modified
	const tstring sIfNoneMatch(request->getHeader("if-none-match"));			// for ETag
	// If-None-Match,ETag
	//const tstring sIfRange = request->getHeader("If-Range");
	//printf("************** If-Modified-Since: %s\n",sIfModifiedSince.c_str());
	//printf("************** If-Range: %s\n",sIfRange.c_str());

	const tstring host(request->getHost());
	//printf("**** host=%s\n",host.c_str());
	CVirtualHost::pointer requestHost = theVirtualHosts.getVirtualHost(host);
	if (requestHost.get() == NULL && theVirtualHosts.getHosts().size() > 1)
	{
		tstring::size_type find = host.find(":");
		if (find != tstring::npos)
		{
			tstring address = host.substr(0, find);
			address.append(":*");
			requestHost = theVirtualHosts.getVirtualHost(address);
			if (requestHost.get() == NULL)
			{
				address = "*";
				address.append(host.substr(find));
				requestHost = theVirtualHosts.getVirtualHost(address);
			}
		}

		if (requestHost.get() != NULL)
		{
			requestHost->setPropertys(theApplication->createAttributes());
			theVirtualHosts.addVirtualHst(host, requestHost);
		}
	}
	if (requestHost.get() != NULL && !requestHost->m_bBuildDocumentRoot)
	{
		if (requestHost->getDocumentRoot().empty())
			requestHost->setDocumentRoot(theApplication->getAppConfPath());
		else
		{
			tstring sDocumentRoot = requestHost->getDocumentRoot();
			namespace fs = boost::filesystem;
			fs::path src_path(sDocumentRoot);
			if (!fs::exists(src_path))
			{
				sDocumentRoot = theApplication->getAppConfPath();
				sDocumentRoot.append("/");
				sDocumentRoot.append(requestHost->getDocumentRoot());
				fs::path src_path2(sDocumentRoot);
				if (!fs::exists(src_path2))
				{
					requestHost.reset();
					CGC_LOG((cgc::LOG_ERROR, "DocumentRoot not exist. %s\n",sDocumentRoot.c_str()));
				}else
					requestHost->setDocumentRoot(sDocumentRoot);
			}
		}
		if (requestHost.get() != NULL)
			requestHost->m_bBuildDocumentRoot = true;
	}

	if (requestHost.get() == NULL)
		requestHost = theDefaultHost;

	tstring sFileName(request->getRequestURI());
	//printf("**** FileName=%s\n",sFileName.c_str());
	if (sFileName == "/")
		sFileName.append(requestHost->getIndex());
	if (sFileName.substr(0,1) != "/")
		sFileName.insert(0, "/");

	// File not exist
	tstring sFilePath(requestHost->getDocumentRoot() + sFileName);
	//printf("**** FilePath=%s,sFileName=%s\n",sFilePath.c_str(),sFileName.c_str());
	namespace fs = boost::filesystem;
	fs::path src_path(sFilePath);
	if (!fs::exists(src_path))
	{
		//printf("**** HTTP Status 404 -1- %s\n",sFileName.c_str());
		response->println("HTTP Status 404 - %s", sFileName.c_str());
		return STATUS_CODE_404;
	}else if (fs::is_directory(src_path))
	{
		// ??
		//printf("**** HTTP Status 404 -2- %s\n",sFileName.c_str());
		response->println("HTTP Status 404 - %s", sFileName.c_str());
		return STATUS_CODE_404;
	}

	//printf(" **** doHttpServer: filename=%s\n",sFileName.c_str());
	tstring sMimeType;
	bool bIsImageOrBinary = false;
	if (isCSPFile(sFilePath,sMimeType,bIsImageOrBinary))
	{
		fs::path src_path(sFilePath);
		const size_t fileSize = (size_t)fs::file_size(src_path);
		const time_t lastTime = fs::last_write_time(src_path);

		bool buildCSPFile = false;
		CCSPFileInfo::pointer fileInfo;
		if (!theFileInfos.find(sFileName, fileInfo))
		{
			if (!theIdleFileInfos.find(sFileName, fileInfo,true))
			{
				buildCSPFile = true;
				fileInfo = CCSPFileInfo::pointer(new CCSPFileInfo(sFileName, fileSize, lastTime));
#ifdef USES_BODB_SCRIPT
				if (theCdbcService.get()!=NULL)
				{
					tstring sFileNameTemp(sFileName);
					theCdbcService->escape_string_in(sFileNameTemp);
					char sql[512];
					sprintf(sql, "INSERT INTO cspfileinfo_t (filename,filesize,lasttime) VALUES('%s',%d,%lld)",sFileNameTemp.c_str(), fileSize, (cgc::bigint)lastTime);
					theCdbcService->execute(sql);
				}
#endif
			}
			theFileInfos.insert(sFileName,fileInfo,false);
		}
		if (!buildCSPFile && fileInfo->isModified(fileSize, lastTime))
		{
			buildCSPFile = true;
			fileInfo->setFileSize(fileSize);
			fileInfo->setLastTime(lastTime);

#ifdef USES_BODB_SCRIPT
			if (theCdbcService.get()!=NULL)
			{
				tstring sFileNameTemp(sFileName);
				theCdbcService->escape_string_in(sFileNameTemp);
				char sql[512];
				sprintf(sql, "UPDATE cspfileinfo_t SET filesize=%d,lasttime=%lld WHERE filename='%s'",fileSize, (cgc::bigint)lastTime, sFileNameTemp.c_str());
				theCdbcService->execute(sql);
			}
#endif
		}
		fileInfo->m_tRequestTime = time(0);	// ***��¼����ʱ�䣬���ڶ�ʱ���̫��û����Դ

		CFileScripts::pointer fileScript;
		if (!theFileScripts.find(sFileName, fileScript))
		{
			buildCSPFile = true;
			fileScript = CFileScripts::pointer(new CFileScripts(sFileName));
			theFileScripts.insert(sFileName, fileScript,false);
		}
		CMycpHttpServer::pointer httpServer = CMycpHttpServer::pointer(new CMycpHttpServer(request, response));
		httpServer->setSystem(theSystem);
		httpServer->setApplication(theApplication);
		httpServer->setFileSystemService(theFileSystemService);
		httpServer->setServiceManager(theServiceManager);
		httpServer->setVirtualHost(requestHost);
		httpServer->setServletName(sFileName.substr(1));
		httpServer->setApps(theApps);
		httpServer->setDSs(theDataSources);

		//printf("**** buildCSPFile=%d\n",(int)(buildCSPFile?1:0));
		if (buildCSPFile)
		{
			if (!fileScript->parserCSPFile(sFilePath.c_str())) return STATUS_CODE_404;

#ifdef USES_BODB_SCRIPT
			if (theCdbcService.get()!=NULL)
			{
				char sql[512];
				tstring sFileNameTemp(sFileName);
				theCdbcService->escape_string_in(sFileNameTemp);
				sprintf(sql, "DELETE FROM scriptitem_t WHERE filename='%s'", sFileNameTemp.c_str());
				theCdbcService->execute(sql);

				const std::vector<CScriptItem::pointer>& scripts = fileScript->getScripts();
				//printf("**** buildCSPFile filename=%s, size=%d\n",sFileName.c_str(),(int)scripts.size());
				char bufferCode[5];
				for (size_t i=0; i<scripts.size(); i++)
				{
					CScriptItem::pointer scriptItem = scripts[i];
					sprintf(bufferCode, "%04d", i);
					insertScriptItem(bufferCode, sFileNameTemp, scriptItem);
				}
			}
		}else if (fileScript->empty() && theCdbcService.get()!=NULL)
		{
			char selectSql[512];
			tstring sFileNameTemp(sFileName);
			theCdbcService->escape_string_in(sFileNameTemp);
			sprintf(selectSql, "SELECT code,sub,itemtype,object1,object2,id,scopy,name,property,type,value FROM scriptitem_t WHERE filename='%s' ORDER BY code", sFileNameTemp.c_str());

			int cdbcCookie = 0;
			//theCdbcService->select(selectSql, cdbcCookie);
			const cgc::bigint ret = theCdbcService->select(selectSql, cdbcCookie);
			//printf("**** %lld=%s\n",ret,selectSql);
			CLockMap<tstring, CScriptItem::pointer> codeScripts;
			cgcValueInfo::pointer record = theCdbcService->first(cdbcCookie);
			while (record.get() != NULL)
			{
				//assert (record->getType() == cgcValueInfo::TYPE_VECTOR);
				//assert (record->size() == 11);
				cgcValueInfo::pointer var_code = record->getVector()[0];
				cgcValueInfo::pointer var_sub = record->getVector()[1];
				cgcValueInfo::pointer var_itemtype = record->getVector()[2];
				cgcValueInfo::pointer var_object1 = record->getVector()[3];
				cgcValueInfo::pointer var_object2 = record->getVector()[4];
				cgcValueInfo::pointer var_id = record->getVector()[5];
				cgcValueInfo::pointer var_scope = record->getVector()[6];
				cgcValueInfo::pointer var_name = record->getVector()[7];
				cgcValueInfo::pointer var_property = record->getVector()[8];
				cgcValueInfo::pointer var_type = record->getVector()[9];
				cgcValueInfo::pointer var_value = record->getVector()[10];
				//const int nvlen = record->getVector()[11]->getIntValue();

				CScriptItem::pointer scriptItem = CScriptItem::pointer(new CScriptItem((CScriptItem::ItemType)var_itemtype->getIntValue()));
				scriptItem->setOperateObject1((CScriptItem::OperateObject)var_object1->getIntValue());
				scriptItem->setOperateObject2((CScriptItem::OperateObject)var_object2->getIntValue());
				scriptItem->setId(var_id->getStr());
				scriptItem->setScope(var_scope->getStr());
				scriptItem->setName(var_name->getStr());
				scriptItem->setProperty(var_property->getStr());
				scriptItem->setType(var_type->getStr());

				std::string sValue(var_value->getStr());
				if (!sValue.empty())
				{
					theCdbcService->escape_string_out(sValue);
					scriptItem->setValue(sValue);
				}
				const tstring scriptCode(var_code->getStr());
				const size_t nScriptCodeSize = scriptCode.size();
				const tstring parentCode(nScriptCodeSize<=4?"":scriptCode.substr(0, nScriptCodeSize-4));
				//printf("**** code=%s;parent_code=%s;value=%s\n",scriptCode.c_str(),parentCode.c_str(),sValue.c_str());
				CScriptItem::pointer parentScriptItem;
				if (!parentCode.empty() && codeScripts.find(parentCode, parentScriptItem))
				{
					const int nSub = var_sub->getIntValue();
					if (nSub == 2)
						parentScriptItem->m_elseif.push_back(scriptItem);
					else if (nSub == 3)
						parentScriptItem->m_else.push_back(scriptItem);
					else
						parentScriptItem->m_subs.push_back(scriptItem);
				}else
				{
					fileScript->addScript(scriptItem);
				}
				codeScripts.insert(scriptCode, scriptItem, false);

				record = theCdbcService->next(cdbcCookie);
			}
			theCdbcService->reset(cdbcCookie);
			if (fileScript->empty())
				fileInfo->setFileSize(0);
#endif
		}

		try
		{
			//printf("**** doIt filename=%s, size=%d\n",sFileName.c_str(),(int)fileScript->getScripts().size());
			const int ret = httpServer->doIt(fileScript);
			if (ret == -1)
				return STATUS_CODE_400;
			statusCode = response->getStatusCode();
		}catch(std::exception const &e)
		{
			theApplication->log(LOG_ERROR, _T("exception, RequestURL \'%s\', lasterror=0x%x\n"), request->getRequestURI().c_str(), GetLastError());
			theApplication->log(LOG_ERROR, _T("'%s'\n"), e.what());
			response->println("EXCEPTION: LastError=%d; %s", e.what());
			return STATUS_CODE_500;
		}catch(...)
		{
			theApplication->log(LOG_ERROR, _T("exception, RequestURL \'%s\', lasterror=0x%x\n"),request->getRequestURI().c_str(), GetLastError());
			response->println("EXCEPTION: LastError=%d", GetLastError());
			return STATUS_CODE_500;
		}
	}else
	{
		// ����
		//HTTP/1.0 200 OK
		//	Content-Length: 13057672
		//	Content-Type: application/octet-stream
		//	Last-Modified: Wed, 10 Oct 2005 00:56:34 GMT
		//	Accept-Ranges: bytes
		//	ETag: "2f38a6cac7cec51:160c"
		//	Server: Microsoft-IIS/6.0
		//	X-Powered-By: ASP.NET
		//	Date: Wed, 16 Nov 2005 01:57:54 GMT
		//	Connection: close 

		// Content-Range: bytes 500-999/1000 
		// Content-Range�ֶ�˵���������������ļ���ĳ����Χ���ļ����ܳ��ȡ���ʱContent-Length�ֶξͲ��������ļ��Ĵ�С�ˣ�
		// ���Ƕ�Ӧ�ļ������Χ���ֽ�������һ��һ��Ҫע�⡣

		// ����
		//Range: bytes=500-      ��ʾ��ȡ���ļ���500-999�ֽڣ���500�ֽڡ�
		//Range: bytes=500-599   ��ʾ��ȡ���ļ���500-599�ֽڣ���100�ֽڡ�

		CResInfo::pointer pResInfo = CGC_OBJECT_CAST<CResInfo>(theAppAttributes->getAttribute(ATTRIBUTE_FILE_INFO, sFilePath));
		if (pResInfo.get() == NULL)
		{
			pResInfo = CResInfo::create(sFilePath,sMimeType);
			theAppAttributes->setAttribute(ATTRIBUTE_FILE_INFO,sFilePath,pResInfo);
		}
		pResInfo->m_tRequestTime = time(0);	// ***��¼����ʱ�䣬���ڶ�ʱ���̫��û����Դ

		fs::path src_path(sFilePath);
		const time_t lastTime = fs::last_write_time(src_path);
		{
			boost::mutex::scoped_lock lock(pResInfo->m_mutex);
			if (pResInfo->m_tModifyTime != lastTime)
			{
				std::fstream stream;
				stream.open(sFilePath.c_str(), std::ios::in|std::ios::binary);
				if (!stream.is_open())
				{
					return STATUS_CODE_404;
				}
				pResInfo->m_nSize = (unsigned int)fs::file_size(src_path);
				pResInfo->m_tModifyTime = lastTime;
				struct tm * tmModifyTime = gmtime(&pResInfo->m_tModifyTime);
				char lpszModifyTime[128];
				strftime(lpszModifyTime, 128, "%a, %d %b %Y %H:%M:%S GMT", tmModifyTime);
				pResInfo->m_sModifyTime = lpszModifyTime;
				//// ETag
				//sprintf(lpszModifyTime,"\"%lld\"",(cgc::bigint)pResInfo->m_tModifyTime);
				//pResInfo->m_sETag = lpszModifyTime;
				// �ȴ���ǰ���ڴ�����
				char * lpszTemp = pResInfo->m_pData;
				pResInfo->m_pData = NULL;
				// ���ļ����ݵ��ڴ棨ֻ��ȡС��50MB��������ֱ�Ӵ��ļ���ȡ��
				if (pResInfo->m_nSize<=const_memory_size)
				{
					char * buffer = new char[pResInfo->m_nSize+1];
					memset(buffer, 0, pResInfo->m_nSize+1);
					stream.seekg(0, std::ios::beg);
					stream.read(buffer, pResInfo->m_nSize);
					buffer[pResInfo->m_nSize] = '\0';
					pResInfo->m_pData = buffer;

					// ����MD5
					MD5 md5;
					md5.update((const unsigned char*)buffer, pResInfo->m_nSize);
					md5.finalize();
					const std::string sFileMd5String = (const char*)md5.hex_digest();
					sprintf(lpszModifyTime,"\"%s\"",sFileMd5String.c_str());
					pResInfo->m_sETag = lpszModifyTime;
				}
				stream.close();
				if (lpszTemp != NULL)
					delete[] lpszTemp;
			}
		}
		if (sIfModifiedSince==pResInfo->m_sModifyTime)
		{
			SetExpiresCache(response,pResInfo->m_tRequestTime,bIsImageOrBinary);
			return STATUS_CODE_304;					// 304 Not Modified
		}
		if (!sIfNoneMatch.empty() && !pResInfo->m_sETag.empty() && sIfNoneMatch.find(pResInfo->m_sETag)!=tstring::npos)
		{
			SetExpiresCache(response,pResInfo->m_tRequestTime,bIsImageOrBinary);
			return STATUS_CODE_304;					// 304 Not Modified
		}

		//printf("**** %s\n",lpszFileName);
		unsigned int nRangeFrom = request->getRangeFrom();
		unsigned int nRangeTo = request->getRangeTo();
		const tstring sRange = request->getHeader(Http_Range);
		//printf("**** %s\n",sRange.c_str());
		//printf("**** Range %d-%d\n",nRangeFrom,nRangeTo);
		if (nRangeTo == 0)
			nRangeTo = pResInfo->m_nSize-1;
		unsigned int nReadTotal = nRangeTo-nRangeFrom+1;				// �������ݳ���
		//if (nReadTotal >= 1024)
		//{
		//	statusCode = STATUS_CODE_206;
		//	nReadTotal = 1024;
		//	nRangeTo = nRangeFrom+1024;
		//}
		if (nRangeTo>=pResInfo->m_nSize)
			return STATUS_CODE_416;					// 416 Requested Range Not Satisfiable
		else if (nReadTotal>const_max_size)	// �ְ�����
		{
			return STATUS_CODE_413;					// 413 Request Entity Too Large
		}else if (!sRange.empty())
		{
			statusCode = STATUS_CODE_206;				// 206 Partial Content
		}
		SetExpiresCache(response,pResInfo->m_tRequestTime,bIsImageOrBinary);
		//const int nExpireSecond = bIsImageOrBinary?(10*const_one_day_seconds):const_one_day_seconds;	// �ı��ļ���10���ӣ�����ͼƬ��=3600=60���ӣ�
		//const time_t tExpiresTime = pResInfo->m_tRequestTime + nExpireSecond;
		//struct tm * tmExpiresTime = gmtime(&tExpiresTime);
		//char lpszBuffer[64];
		//strftime(lpszBuffer, 64, "%a, %d %b %Y %H:%M:%S GMT", tmExpiresTime);	// Tue, 19 Aug 2015 07:00:19 GMT
		//response->setHeader("Expires",lpszBuffer);
		//sprintf(lpszBuffer, "max-age=%d", nExpireSecond);
		//response->setHeader("Cache-Control",lpszBuffer);
		////response->setHeader("Cache-Control","max-age=8640000");	// 86400=1�죻100��

		//char lpszBuffer[512];
		//sprintf(lpszBuffer,"attachment;filename=%s%s",sResourceId.c_str(),sExt.c_str());	// ������ʽ����
		//sprintf(lpszBuffer,"inline;filename=%s%s",sResourceId.c_str(),sExt.c_str());		// ��ҳ��
		//response->setHeader(Http_ContentDisposition,lpszBuffer);
		if (pResInfo->m_pData!=NULL && !pResInfo->m_sETag.empty())
			response->setHeader("ETag",pResInfo->m_sETag);
		response->setHeader("Last-Modified",pResInfo->m_sModifyTime);
		response->setContentType(sMimeType);
		if (statusCode == STATUS_CODE_206)
		{
			response->setHeader(Http_AcceptRanges,"bytes");
		}
//		if (statusCode == STATUS_CODE_206)
//		{
//			//HTTP/1.1 200 OK  
//			//Transfer-Encoding: chunked  
//			//...  
//			//5  
//			//Hello  
//			//6  
//			//world!  
//			//0 
//			response->setHeader("Transfer-Encoding","chunked");	// �ֶδ���
//
//			response->setHeader(Http_AcceptRanges,"bytes");
//			response->setHeader("ETag",sFilePath);
//			response->setHeader("Connection","Keep-Alive");
//			response->setContentType(sMimeType);
//			//response->setContentType("multipart/byteranges");
//			//response->setContentType("application/octet-stream");	// ����ʾ����
//			while(!response->isInvalidate())
//			{
//				printf("**** read from memory: %d-%d/%d\n",nRangeFrom,nRangeTo,pResInfo->m_nSize);
//				sprintf(lpszBuffer,"bytes %d-%d/%d",nRangeFrom,nRangeTo,pResInfo->m_nSize);
//				response->setHeader(Http_ContentRange,lpszBuffer);
//				response->writeData(pResInfo->m_pData+nRangeFrom, nReadTotal);
//				if (nRangeFrom == 0)
//					response->sendResponse(STATUS_CODE_200);
//				else if (nRangeTo>=(pResInfo->m_nSize-1))
//				{
//					response->sendResponse(STATUS_CODE_200);
//					break;
//				}else
//					response->sendResponse(STATUS_CODE_206);
//				nRangeFrom += nReadTotal;
//				nRangeTo += nReadTotal;
//				if (nRangeTo>pResInfo->m_nSize-1)
//					nRangeTo=pResInfo->m_nSize-1;
//#ifdef WIN32
//				Sleep(100);
//#else
//				usleep(100000);
//#endif
//			}
//			return statusCode;;
//		}else
//		{
//			response->setContentType(sMimeType);
//		}
		if (nReadTotal > 0)
		{
			//printf("**** resid=%lld,file=%s\n",sResourceId,pResInfo->m_sFileName.c_str());
			if (pResInfo->m_pData == NULL)
			{
				// ���ļ�
				//printf("**** read from file: %d-%d/%d\n",nRangeFrom,nRangeTo,pResInfo->m_nSize);
				std::fstream stream;
				stream.open(pResInfo->m_sFileName.c_str(), std::ios::in|std::ios::binary);
				if (!stream.is_open())
				{
					return STATUS_CODE_404;
				}
				stream.seekg(nRangeFrom, std::ios::beg);
				char * buffer = new char[nReadTotal+1];
				memset(buffer, 0, nReadTotal+1);
				stream.read(buffer, nReadTotal);
				stream.clear();
				stream.close();
				buffer[nReadTotal] = '\0';
				response->writeData(buffer, nReadTotal);
				// ����MD5
				MD5 md5;
				md5.update((const unsigned char*)buffer, nReadTotal);
				md5.finalize();
				const std::string sFileMd5String = (const char*)md5.hex_digest();
				sprintf(buffer,"\"%s\"",sFileMd5String.c_str());
				pResInfo->m_sETag = buffer;
				response->setHeader("ETag",pResInfo->m_sETag);
				delete[] buffer;
			}else
			{
				// ���ڴ�
				//printf("**** read from memory: %d-%d/%d\n",nRangeFrom,nRangeTo,pResInfo->m_nSize);
				try
				{
					response->writeData(pResInfo->m_pData+nRangeFrom, nReadTotal);
				}catch(std::exception const &)
				{
					return STATUS_CODE_500;
				}catch(...)
				{
					return STATUS_CODE_500;
				}
			}
		}else
		{
			// ?
		}
	//	tfstream stream;
	//	stream.open(sFilePath.c_str(), std::ios::in|std::ios::binary);
	//	if (!stream.is_open())
	//	{
	//		return STATUS_CODE_404;
	//	}
	//	int nRangeFrom = request->getRangeFrom();
	//	int nRangeTo = request->getRangeTo();
	//	const tstring sRange = request->getHeader(Http_Range);
	//	//printf("**** %s\n",sRange.c_str());
	//	//printf("**** Range %d-%d\n",nRangeFrom,nRangeTo);
	//	stream.seekg(0, std::ios::end);
	//	int nTotal = stream.tellg();
	//	if (nRangeTo == 0)
	//		nRangeTo = nTotal;
	//	int nReadTotal = nRangeTo-nRangeFrom;				// �������ݳ���
	//	if (nReadTotal > const_max_size)					// �ְ�����
	//	{
	//		return STATUS_CODE_413;							// 413 Request Entity Too Large
	//		nReadTotal = const_max_size;
	//		statusCode = STATUS_CODE_206;
	//	}else if (!sRange.empty())
	//	{
	//		statusCode = STATUS_CODE_206;
	//	}
	//	char lpszBuffer[512];
	//	//sprintf(lpszBuffer,"attachment;filename=%s%s",sResourceId.c_str(),sExt.c_str());	// ������ʽ����
	//	//sprintf(lpszBuffer,"inline;filename=%s%s",sResourceId.c_str(),sExt.c_str());		// ��ҳ��
	//	//response->setHeader(Http_ContentDisposition,lpszBuffer);
	//	if (statusCode == STATUS_CODE_206)
	//	{
	//		sprintf(lpszBuffer,"bytes %d-%d/%d",nRangeFrom,nRangeTo,nTotal);
	//		response->setHeader(Http_ContentRange,lpszBuffer);
	//	}
	//	response->setHeader("Last-Modified",lpszModifyTime);
	//	response->setHeader("Cache-Control","max-age=8640000");	// 86400=1��
	//	response->setHeader(Http_AcceptRanges,"bytes");
	//	response->setContentType(sMimeType);
	//	if (nReadTotal > 0)
	//	{
	//		stream.seekg(nRangeFrom, std::ios::beg);
	//		char * buffer = new char[nReadTotal+1];
	//		memset(buffer, 0, nReadTotal+1);
	//		stream.read(buffer, nReadTotal);
	//		buffer[nReadTotal] = '\0';
	//		response->writeData(buffer, nReadTotal);
	//		delete[] buffer;
	//	}else
	//	{
	//		// ?
	//	}
	//	//nTotal = nRangeTo-nRangeFrom;	// �������ݳ���
	//	////nTotal = std::min(2*1024,nRangeTo-nRangeFrom);	// �������ݳ���

	//	//// STATUS_CODE_100

	//	//response->setHeader("Accept-Ranges","bytes");
	//	//response->setContentType(sMimeType);
	//	//if (nTotal > 0)
	//	//{
	//	//	stream.seekg(nRangeFrom, std::ios::beg);
	//	//	char * buffer = new char[nTotal+1];
	//	//	memset(buffer, 0, nTotal+1);
	//	//	stream.read(buffer, nTotal);
	//	//	buffer[nTotal] = '\0';
	//	//	response->writeData(buffer, nTotal);
	//	//	delete[] buffer;
	//	//}else
	//	//{
	//	//	// ?
	//	//}

	//	stream.clear();
	//	stream.close();
	}

	//printf("**** return =%d\n",statusCode);
	return statusCode;
}
