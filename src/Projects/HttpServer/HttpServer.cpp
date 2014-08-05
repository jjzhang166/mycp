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

//#define USES_BODB_SCRIPT

#ifdef USES_BODB_SCRIPT
cgcCDBCService::pointer theCdbcService;
#endif
cgcServiceInterface::pointer theFileSystemService;
XmlParseHosts theVirtualHosts;
XmlParseApps::pointer theApps;
XmlParseDSs::pointer theDataSources;
CVirtualHost::pointer theDefaultHost;
CLockMap<tstring, CFileScripts::pointer> theFileScripts;
CLockMap<tstring, CCSPFileInfo::pointer> theFileInfos;
cgcAttributes::pointer theAppAttributes;
const int ATTRIBUTE_FILE_INFO = 8;				// filepath->

extern "C" bool CGC_API CGC_Module_Init(void)
{
	theFileSystemService = theServiceManager->getService("FileSystemService");
	if (theFileSystemService.get() == NULL)
	{
		CGC_LOG((cgc::LOG_ERROR, "FileSystemService Error.\n"));
		return false;
	}

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

#ifdef USES_BODB_SCRIPT
	tstring cdbcDataSource = initParameters->getParameterValue("CDBCDataSource", "ds_httpserver");
	theCdbcService = theServiceManager->getCDBDService(cdbcDataSource);
	if (theCdbcService.get() == NULL)
	{
		CGC_LOG((cgc::LOG_ERROR, "HttpServer DataSource Error.\n"));
		return false;
	}
	// 
	int cdbcCookie = 0;
	theCdbcService->select("SELECT filename,filesize,lasttime FROM cspfileinfo_t", cdbcCookie);
	if (cdbcCookie != 0)
	{
		cgcValueInfo::pointer record = theCdbcService->first(cdbcCookie);
		while (record.get() != NULL)
		{
			assert (record->getType() == cgcValueInfo::TYPE_VECTOR);
			assert (record->size() == 3);

			cgcValueInfo::pointer var_filename = record->getVector()[0];
			cgcValueInfo::pointer var_filesize = record->getVector()[1];
			var_filesize->totype(cgcValueInfo::TYPE_INT);
			cgcValueInfo::pointer var_lasttime = record->getVector()[2];
			var_lasttime->totype(cgcValueInfo::TYPE_BIGINT);

			CCSPFileInfo::pointer fileInfo = CSP_FILEINFO(var_filename->getStr(), var_filesize->getInt(), var_lasttime->getBigInt());
			theFileInfos.insert(fileInfo->getFileName(), fileInfo);

			record = theCdbcService->next(cdbcCookie);
		}

		theCdbcService->reset(cdbcCookie);
	}
#endif

	theAppAttributes = theApplication->getAttributes(true);
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
	if (theAppAttributes.get())
	{
		theAppAttributes->clearAllAtrributes();
		theAppAttributes.reset();
	}

#ifdef USES_BODB_SCRIPT
	theCdbcService.reset();
#endif
	theFileSystemService.reset();
	theDefaultHost.reset();
	theVirtualHosts.clear();
	theApps.reset();
	theDataSources.reset();
	theFileScripts.clear();
	theFileInfos.clear();
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

bool isCSPFile(const tstring& filename, tstring& outMimeType)
{
	outMimeType = "text/html";
	tstring::size_type find = filename.rfind(".");
	if (find == tstring::npos)
	{
		outMimeType = "application/octet-stream";
		return false;
	}

	tstring ext = filename.substr(find+1);
	if (ext == "csp")
		return true;
	else if (ext == "gif")
		outMimeType = "image/gif";
	else if(ext == "jpeg" || ext == "jpg" || ext == "jpe")
		outMimeType = "image/jpeg";
	else if(ext == "htm" || ext == "html" || ext == "shtml")
		outMimeType = "text/html";
	else if(ext == "js")
		outMimeType = "text/javascript";
	else if(ext == "css")
		outMimeType = "text/css";
	else if(ext == "txt")
		outMimeType = "text/plain";
	else if(ext == "xls" || ext == "xla")
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

	return false;
}
#ifdef USES_BODB_SCRIPT
void insertScriptItem(const tstring& code, const tstring & sFileName, CScriptItem::pointer scriptItem, int sub = 0)
{
	std::string sValue = scriptItem->getValue();
	theCdbcService->escape_string_in(sValue);
	int sqlsize = sValue.size()+1000;
	char * sql = new char[sqlsize];

	sprintf(sql, "INSERT INTO scriptitem_t (filename,code,sub,itemtype,object1,object2,id,scopy,name,property,type,value) \
				 VALUES('%s','%s',%d,%d,%d,%d,'%s','%s','%s','%s','%s','%s')",
				 sFileName.c_str(), code.c_str(), sub,(int)scriptItem->getItemType(),
				 (int)scriptItem->getOperateObject1(),(int)scriptItem->getOperateObject2(),
				 scriptItem->getId().c_str(), scriptItem->getScope().c_str(),
				 scriptItem->getName().c_str(), scriptItem->getProperty().c_str(),
				 scriptItem->getType().c_str(), sValue.c_str()
				 );
	theCdbcService->execute(sql);
	delete[] sql;

	for (size_t i=0; i<scriptItem->m_subs.size(); i++)
	{
		CScriptItem::pointer subScriptItem = scriptItem->m_subs[i];

		char bufferCode[32];
		sprintf(bufferCode, "%s%03d", code.c_str(), i);
		insertScriptItem(bufferCode, sFileName, subScriptItem, 1);
	}
	for (size_t i=0; i<scriptItem->m_elseif.size(); i++)
	{
		CScriptItem::pointer subScriptItem = scriptItem->m_elseif[i];

		char bufferCode[32];
		sprintf(bufferCode, "%s%03d", code.c_str(), i);
		insertScriptItem(bufferCode, sFileName, subScriptItem, 2);
	}
	for (size_t i=0; i<scriptItem->m_else.size(); i++)
	{
		CScriptItem::pointer subScriptItem = scriptItem->m_else[i];

		char bufferCode[32];
		sprintf(bufferCode, "%s%03d", code.c_str(), i);
		insertScriptItem(bufferCode, sFileName, subScriptItem, 3);
	}
}
#endif

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
	std::string m_sFileName;
	std::string m_sMimeType;
	//std::fstream m_fs;
	time_t m_tModifyTime;
	std::string m_sModifyTime;
	time_t m_tRequestTime;
	unsigned int m_nSize;
	char * m_pData;
};


const int const_memory_size = 50*1024*1024;		// max 50MB
const int const_max_size = 21*1024*1024;		// max 21MB
extern "C" HTTP_STATUSCODE CGC_API doHttpServer(const cgcHttpRequest::pointer & request, const cgcHttpResponse::pointer& response)
{
	HTTP_STATUSCODE statusCode = STATUS_CODE_200;
	const tstring sIfModifiedSince = request->getHeader("If-Modified-Since");
	//const tstring sIfRange = request->getHeader("If-Range");
	//const tstring sETag = request->getHeader("ETag");
	//printf("************** If-Modified-Since: %s\n",sIfModifiedSince.c_str());
	//printf("************** If-Range: %s\n",sIfRange.c_str());
	//printf("************** ETag: %s\n",sETag.c_str());

	tstring host = request->getHost();
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
					sDocumentRoot.append(requestHost->getDocumentRoot());
					fs::path src_path2(sDocumentRoot);
					if (!fs::exists(src_path2))
						requestHost.reset();
					else
						requestHost->setDocumentRoot(sDocumentRoot);
				}
			}
			if (requestHost.get() != NULL)
			{
				requestHost->setPropertys(theApplication->createAttributes());
				theVirtualHosts.addVirtualHst(host, requestHost);
			}
		}
	}

	if (requestHost.get() == NULL)
		requestHost = theDefaultHost;

	tstring sFileName(request->getRequestURI());
	if (sFileName == "/")
		sFileName.append(requestHost->getIndex());
	if (sFileName.substr(0,1) != "/")
		sFileName.insert(0, "/");

	// File not exist
	tstring sFilePath = requestHost->getDocumentRoot() + sFileName;
	namespace fs = boost::filesystem;
	fs::path src_path(sFilePath);
	if (!fs::exists(src_path))
	{
		response->println("%s: NOT FOUND", sFileName.c_str());
		return STATUS_CODE_404;
	}

	//printf(" **** doHttpServer: filename=%s\n",sFileName.c_str());
	tstring sMimeType = "";
	if (isCSPFile(sFilePath, sMimeType))
	{
		fs::path src_path(sFilePath);
		size_t fileSize = (size_t)fs::file_size(src_path);
		time_t lastTime = fs::last_write_time(src_path);

		bool buildCSPFile = false;
		CCSPFileInfo::pointer fileInfo;
		if (!theFileInfos.find(sFileName, fileInfo))
		{
			fileInfo = CCSPFileInfo::pointer(new CCSPFileInfo(sFileName, fileSize, lastTime));
			theFileInfos.insert(sFileName, fileInfo);
			buildCSPFile = true;

#ifdef USES_BODB_SCRIPT
			char sql[512];
#ifdef WIN32
			sprintf(sql, "INSERT INTO cspfileinfo_t (filename,filesize,lasttime) VALUES('%s',%d,%I64d)",
#else
			sprintf(sql, "INSERT INTO cspfileinfo_t (filename,filesize,lasttime) VALUES('%s',%d,%ld)",
#endif
						 sFileName.c_str(), fileSize, lastTime);
			theCdbcService->execute(sql);
#endif
		}else if (fileInfo->isModified(fileSize, lastTime))
		{
			buildCSPFile = true;
			fileInfo->setFileSize(fileSize);
			fileInfo->setLastTime(lastTime);

#ifdef USES_BODB_SCRIPT
			char sql[512];
#ifdef WIN32
			sprintf(sql, "UPDATE cspfileinfo_t SET filesize=%d,lasttime=%I64d WHERE filename='%s')",
#else
			sprintf(sql, "UPDATE cspfileinfo_t SET filesize=%d,lasttime=%ld WHERE filename='%s')",
#endif
						 fileSize, lastTime, sFileName.c_str());
			theCdbcService->execute(sql);
#endif
		}

		CFileScripts::pointer fileScript;
		if (!theFileScripts.find(sFileName, fileScript))
		{
			fileScript = CFileScripts::pointer(new CFileScripts(sFileName));
			theFileScripts.insert(sFileName, fileScript);
		}

		CMycpHttpServer::pointer httpServer = CMycpHttpServer::pointer(new CMycpHttpServer(request, response));
		httpServer->setApplication(theApplication);
		httpServer->setFileSystemService(theFileSystemService);
		httpServer->setServiceManager(theServiceManager);
		httpServer->setVirtualHost(requestHost);
		httpServer->setServletName(sFileName.substr(1));
		httpServer->setApps(theApps);
		httpServer->setDSs(theDataSources);

		if (buildCSPFile)
		{
			if (!fileScript->parserCSPFile(sFilePath.c_str())) return STATUS_CODE_404;

#ifdef USES_BODB_SCRIPT
			char sql[512];
			theCdbcService->escape_string_in(sFileName);
			sprintf(sql, "DELETE FROM scriptitem_t WHERE filename='%s'", sFileName.c_str());
			theCdbcService->execute(sql);

			const std::vector<CScriptItem::pointer>& scripts = fileScript->getScripts();
			for (size_t i=0; i<scripts.size(); i++)
			{
				CScriptItem::pointer scriptItem = scripts[i];
				char bufferCode[4];
				sprintf(bufferCode, "%03d", i);
				insertScriptItem(bufferCode, sFileName, scriptItem);
			}
		}else if (fileScript->getScripts().empty())
		{
			char selectSql[512];
			theCdbcService->escape_string_in(sFileName);
			sprintf(selectSql, "SELECT code,sub,itemtype,object1,object2,id,scopy,name,property,type,value FROM scriptitem_t WHERE filename='%s'", sFileName.c_str());

			int cdbcCookie = 0;
			int ret = theCdbcService->select(selectSql, cdbcCookie);
			printf("**** %d=%s\n",ret,selectSql);
			if (cdbcCookie != 0)
			{
				CLockMap<tstring, CScriptItem::pointer> codeScripts;

				//std::vector<CScriptItem::pointer> scripts;

				cgcValueInfo::pointer record = theCdbcService->first(cdbcCookie);
				while (record.get() != NULL)
				{
					assert (record->getType() == cgcValueInfo::TYPE_VECTOR);
					assert (record->size() == 11);

					cgcValueInfo::pointer var_code = record->getVector()[0];
					cgcValueInfo::pointer var_sub = record->getVector()[1];
					var_sub->totype(cgcValueInfo::TYPE_INT);
					cgcValueInfo::pointer var_itemtype = record->getVector()[2];
					var_itemtype->totype(cgcValueInfo::TYPE_INT);
					cgcValueInfo::pointer var_object1 = record->getVector()[3];
					var_object1->totype(cgcValueInfo::TYPE_INT);
					cgcValueInfo::pointer var_object2 = record->getVector()[4];
					var_object2->totype(cgcValueInfo::TYPE_INT);
					cgcValueInfo::pointer var_id = record->getVector()[5];
					cgcValueInfo::pointer var_scope = record->getVector()[6];
					cgcValueInfo::pointer var_name = record->getVector()[7];
					cgcValueInfo::pointer var_property = record->getVector()[8];
					cgcValueInfo::pointer var_type = record->getVector()[9];
					cgcValueInfo::pointer var_value = record->getVector()[10];

					CScriptItem::pointer scriptItem = CScriptItem::pointer(new CScriptItem((CScriptItem::ItemType)var_itemtype->getInt()));
					scriptItem->setOperateObject1((CScriptItem::OperateObject)var_object1->getInt());
					scriptItem->setOperateObject2((CScriptItem::OperateObject)var_object2->getInt());
					scriptItem->setId(var_id->getStr());
					scriptItem->setScope(var_scope->getStr());
					scriptItem->setName(var_name->getStr());
					scriptItem->setProperty(var_property->getStr());
					scriptItem->setType(var_type->getStr());

					std::string sValue = var_value->getStr();
					theCdbcService->escape_string_out(sValue);
					scriptItem->setValue(sValue);

					const tstring scriptCode = var_code->getStr();
					const tstring parentCode = scriptCode.size()<=3?"":scriptCode.substr(0, scriptCode.size()-3);
					printf("**** code=%s;parent_code=%s;value=%s\n",scriptCode.c_str(),parentCode.c_str(),sValue.c_str());
					CScriptItem::pointer parentScriptItem;
					if (codeScripts.find(parentCode, parentScriptItem))
					{
						if (var_sub->getInt() == 2)
							parentScriptItem->m_elseif.push_back(scriptItem);
						else if (var_sub->getInt() == 3)
							parentScriptItem->m_else.push_back(scriptItem);
						else
							parentScriptItem->m_subs.push_back(scriptItem);
					}else
					{
						fileScript->addScript(scriptItem);
					}
					codeScripts.insert(scriptCode, scriptItem);

					record = theCdbcService->next(cdbcCookie);
				}

				theCdbcService->reset(cdbcCookie);
			}
#endif
		}

		try
		{
			int ret = httpServer->doIt(fileScript);
			if (ret == -1)
				return STATUS_CODE_400;
			statusCode = response->getStatusCode();
		}catch(std::exception const &e)
		{
			theApplication->log(LOG_ERROR, _T("exception, RequestURL \'%s\', lasterror=0x%x\n"),
				request->getRequestURI().c_str(), GetLastError());
			theApplication->log(LOG_ERROR, _T("'%s'\n"), e.what());
			response->println("EXCEPTION: LastError=%d; %s", e.what());
			return STATUS_CODE_500;
		}catch(...)
		{
			theApplication->log(LOG_ERROR, _T("exception, RequestURL \'%s\', lasterror=0x%x\n"),
				request->getRequestURI().c_str(), GetLastError());
			response->println("EXCEPTION: LastError=%d", GetLastError());
			return STATUS_CODE_500;
		}
	}else
	{
		// 返回
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
		// Content-Range字段说明服务器返回了文件的某个范围及文件的总长度。这时Content-Length字段就不是整个文件的大小了，
		// 而是对应文件这个范围的字节数，这一点一定要注意。

		// 请求
		//Range: bytes=500-      表示读取该文件的500-999字节，共500字节。
		//Range: bytes=500-599   表示读取该文件的500-599字节，共100字节。

		CResInfo::pointer pResInfo = CGC_OBJECT_CAST<CResInfo>(theAppAttributes->getAttribute(ATTRIBUTE_FILE_INFO, sFilePath));
		if (pResInfo.get() == NULL)
		{
			pResInfo = CResInfo::create(sFilePath,sMimeType);
			theAppAttributes->setAttribute(ATTRIBUTE_FILE_INFO,sFilePath,pResInfo);
		}
		pResInfo->m_tRequestTime = time(0);	// ???记录最新时间，用于定时清空太久没用资源

		fs::path src_path(sFilePath);
		time_t lastTime = fs::last_write_time(src_path);
		if (pResInfo->m_tModifyTime != lastTime)
		{
			pResInfo->m_nSize = (size_t)fs::file_size(src_path);
			pResInfo->m_tModifyTime = lastTime;
			struct tm * tmModifyTime = gmtime(&pResInfo->m_tModifyTime);
			char lpszModifyTime[128];
			strftime(lpszModifyTime, 128, "%a, %d %b %Y %H:%M:%S GMT", tmModifyTime);
			pResInfo->m_sModifyTime = lpszModifyTime;
			std::fstream stream;
			stream.open(sFilePath.c_str(), std::ios::in|std::ios::binary);
			if (!stream.is_open())
			{
				return STATUS_CODE_404;
			}
			// 先处理前面内存数据
			char * lpszTemp = pResInfo->m_pData;
			pResInfo->m_pData = NULL;
			if (lpszTemp != NULL)
				delete[] lpszTemp;
			// 读文件内容到内存（只读取小于50MB，超过的直接从文件读取）
			if (pResInfo->m_nSize<=const_memory_size)
			{
				char * buffer = new char[pResInfo->m_nSize+1];
				memset(buffer, 0, pResInfo->m_nSize+1);
				stream.seekg(0, std::ios::beg);
				stream.read(buffer, pResInfo->m_nSize);
				buffer[pResInfo->m_nSize] = '\0';
				pResInfo->m_pData = buffer;
			}
			stream.close();
		}
		if (sIfModifiedSince==pResInfo->m_sModifyTime)
		{
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
		unsigned int nReadTotal = nRangeTo-nRangeFrom+1;				// 重设数据长度
		//if (nReadTotal >= 1024)
		//{
		//	statusCode = STATUS_CODE_206;
		//	nReadTotal = 1024;
		//	nRangeTo = nRangeFrom+1024;
		//}
		if (nRangeTo>=pResInfo->m_nSize)
			return STATUS_CODE_416;					// 416 Requested Range Not Satisfiable
		else if (nReadTotal>const_max_size)	// 分包下载
		{
			return STATUS_CODE_413;					// 413 Request Entity Too Large
		}else if (!sRange.empty())
		{
			statusCode = STATUS_CODE_206;				// 206 Partial Content
		}

		//char lpszBuffer[512];
		//sprintf(lpszBuffer,"attachment;filename=%s%s",sResourceId.c_str(),sExt.c_str());	// 附件方式下载
		//sprintf(lpszBuffer,"inline;filename=%s%s",sResourceId.c_str(),sExt.c_str());		// 网页打开
		//response->setHeader(Http_ContentDisposition,lpszBuffer);
		response->setHeader("Last-Modified",pResInfo->m_sModifyTime);
		response->setHeader("Cache-Control","max-age=8640000");	// 86400=1天；100天
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
//			response->setHeader("Transfer-Encoding","chunked");	// 分段传输
//
//			response->setHeader(Http_AcceptRanges,"bytes");
//			response->setHeader("ETag",sFilePath);
//			response->setHeader("Connection","Keep-Alive");
//			response->setContentType(sMimeType);
//			//response->setContentType("multipart/byteranges");
//			//response->setContentType("application/octet-stream");	// 会提示下载
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
				// 读文件
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
				delete[] buffer;
			}else
			{
				// 读内存
				//printf("**** read from memory: %d-%d/%d\n",nRangeFrom,nRangeTo,pResInfo->m_nSize);
				response->writeData(pResInfo->m_pData+nRangeFrom, nReadTotal);
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
	//	int nReadTotal = nRangeTo-nRangeFrom;				// 重设数据长度
	//	if (nReadTotal > const_max_size)					// 分包下载
	//	{
	//		return STATUS_CODE_413;							// 413 Request Entity Too Large
	//		nReadTotal = const_max_size;
	//		statusCode = STATUS_CODE_206;
	//	}else if (!sRange.empty())
	//	{
	//		statusCode = STATUS_CODE_206;
	//	}
	//	char lpszBuffer[512];
	//	//sprintf(lpszBuffer,"attachment;filename=%s%s",sResourceId.c_str(),sExt.c_str());	// 附件方式下载
	//	//sprintf(lpszBuffer,"inline;filename=%s%s",sResourceId.c_str(),sExt.c_str());		// 网页打开
	//	//response->setHeader(Http_ContentDisposition,lpszBuffer);
	//	if (statusCode == STATUS_CODE_206)
	//	{
	//		sprintf(lpszBuffer,"bytes %d-%d/%d",nRangeFrom,nRangeTo,nTotal);
	//		response->setHeader(Http_ContentRange,lpszBuffer);
	//	}
	//	response->setHeader("Last-Modified",lpszModifyTime);
	//	response->setHeader("Cache-Control","max-age=8640000");	// 86400=1天
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
	//	//nTotal = nRangeTo-nRangeFrom;	// 重设数据长度
	//	////nTotal = std::min(2*1024,nRangeTo-nRangeFrom);	// 重设数据长度

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

	return statusCode;
}
