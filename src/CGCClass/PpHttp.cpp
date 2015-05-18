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

#include "PpHttp.h"
#include "Base64.h"
//#include <boost/format.hpp>

namespace cgc
{
const size_t MAX_HTTPHEAD_SIZE		= 2*1024;
const size_t INCREASE_BODY_SIZE		= 20*1024;
const char * SERVERNAME		= "MYCP Http Server/1.0";

CPpHttp::CPpHttp(void)
: m_host("127.0.0.1"), m_account(""), m_secure(""), m_moduleName(""), m_functionName("doHttpFunc"), m_httpVersion("HTTP/1.1"),m_restVersion("v01"), m_contentLength(0), m_method(HTTP_NONE)
, m_requestURL(""), m_requestURI(""), m_queryString(""),m_postString(""), m_fileName("")
, m_nRangeFrom(0), m_nRangeTo(0)
, m_keepAlive(true), m_keepAliveInterval(0), /*m_contentData(NULL), */m_contentSize(0),m_receiveSize(0)/*,m_nCookieExpiresMinute(0)*/
, m_statusCode(STATUS_CODE_200), m_addDateHeader(false),m_addContentLength(true), m_sReqContentType(""), m_sResContentType("text/html"), m_sLocation("")
, m_forwardFromURL("")
, m_pHeaderBufferTemp(NULL), m_pHeaderTemp(NULL), m_pCookieTemp(NULL)
{
	m_bodySize = 0;
	m_bodyBufferSize = INCREASE_BODY_SIZE;
	m_resultBuffer = new char[MAX_HTTPHEAD_SIZE+m_bodyBufferSize+1];
}
CPpHttp::~CPpHttp(void)
{
	//m_multiparts.clear();
	m_propertys.cleanAllPropertys();
	m_pReqHeaders.cleanAllPropertys();
	m_pReqCookies.cleanAllPropertys();

	//if (m_contentData)
	//	delete[] m_contentData;
	delete[] m_resultBuffer;
	if (m_pHeaderBufferTemp!=NULL)
		delete[] m_pHeaderBufferTemp;
	if (m_pHeaderTemp!=NULL)
		delete[] m_pHeaderTemp;
	if (m_pCookieTemp!=NULL)
		delete[] m_pCookieTemp;

	if (m_fileSystemService.get() != NULL)
	{
		for (size_t i=0; i<m_files.size(); i++)
		{
			m_fileSystemService->callService("delete", CGC_VALUEINFO(m_files[i]->getUploadFile()->getFilePath()));
		}
	}
	m_files.clear();
}

bool CPpHttp::getUploadFile(std::vector<cgcUploadFile::pointer>& outFiles) const
{
	for (size_t i=0; i<m_files.size(); i++)
	{
		outFiles.push_back(m_files[i]->getUploadFile());
	}
	return !m_files.empty();
	//return m_currentMultiPart.get() == NULL ? cgcNullUploadFile : m_currentMultiPart->getUploadFile();
}

tstring CPpHttp::getCookie(const tstring & name, const tstring& defaultValue) const
{
	const cgcValueInfo::pointer valueInfo = m_pReqCookies.getProperty(name);
	return valueInfo.get() == NULL ? defaultValue : valueInfo->getStr();
}

tstring CPpHttp::getHeader(const tstring & header, const tstring& defaultValue) const
{
	const cgcValueInfo::pointer valueInfo = m_pReqHeaders.getProperty(header);
	return valueInfo.get() == NULL ? defaultValue : valueInfo->getStr();
}

/////////////////////////////////////////////
// Response
void CPpHttp::println(const char * text, size_t size)
{
	write(text, size);
	write("\r\n", 2);
}
void CPpHttp::println(const tstring& text)
{
	write(text.c_str(), text.size());
	write("\r\n", 2);
}
void CPpHttp::write(const char * text, size_t size)
{
	if (text == NULL || size == std::string::npos) return;

	if (m_method == HTTP_HEAD) return;

	if (m_bodySize+size > m_bodyBufferSize)
	{
		char * bufferTemp = NULL;
		if (m_bodySize>0)
		{
			bufferTemp = new char[m_bodySize+1];
			memcpy(bufferTemp, m_resultBuffer+MAX_HTTPHEAD_SIZE, m_bodySize);
		}
		delete[] m_resultBuffer;

		m_bodyBufferSize += (size > INCREASE_BODY_SIZE ? size : INCREASE_BODY_SIZE);
		m_resultBuffer = new char[MAX_HTTPHEAD_SIZE+m_bodyBufferSize+1];
		if (m_resultBuffer==NULL) // *
			m_resultBuffer = new char[MAX_HTTPHEAD_SIZE+m_bodyBufferSize+1];
		if (bufferTemp!=NULL)
		{
			memcpy(m_resultBuffer+MAX_HTTPHEAD_SIZE, bufferTemp, m_bodySize);
			delete[] bufferTemp;
		}
	}
	memcpy(m_resultBuffer+MAX_HTTPHEAD_SIZE+m_bodySize, text, size);
	m_bodySize += size;
	m_resultBuffer[MAX_HTTPHEAD_SIZE+m_bodySize] = '\0';
}
void CPpHttp::write(const tstring& text)
{
	write(text.c_str(), text.size());
}
void CPpHttp::newline(void)
{
	write("\r\n", 2);
}
void CPpHttp::reset(void)
{
	//m_keepAliveInterval	// ***这个不能重设
	m_statusCode = STATUS_CODE_200;
	m_pResHeaders.clear();
	//m_sMyCookieSessionId.clear();
	//m_nCookieExpiresMinute = 0;
	m_pResCookies.clear();
	m_sResContentType = "text/html";
	m_sLocation = "";
	m_moduleName = "";
	m_functionName = "";
	m_addDateHeader = false;
	m_addContentLength = true;
	m_bodySize = 0;
	memset(m_resultBuffer, 0, m_bodyBufferSize);
}
void CPpHttp::init(void)
{
	reset();
	m_sMyCookieSessionId = "";
	m_files.clear();
	m_currentMultiPart.reset();
	m_sCurrentParameterData = "";
}

	
void CPpHttp::setCookieMySessionId(const tstring& sMySessionId)
{
	//bool bNewMySessionId = (bool)(m_sMyCookieSessionId != sMySessionId);
	m_sMyCookieSessionId = sMySessionId;
	//if (bNewMySessionId)
	{
		setCookie(cgcCookieInfo::create(Http_CookieSessionId,m_sMyCookieSessionId));
	}
}

void CPpHttp::setCookie(const tstring& name, const tstring& value)
{
	if (!name.empty())
	{
		//m_pResCookies.remove(name);
		m_pResCookies.insert(name,cgcCookieInfo::create(name,value));
	}
}
void CPpHttp::setCookie(const cgcCookieInfo::pointer& pCookieInfo)
{
	if (pCookieInfo.get()!= NULL && !pCookieInfo->m_sCookieName.empty())
	{
		//m_pResCookies.remove(pCookieInfo->m_sCookieName);
		m_pResCookies.insert(pCookieInfo->m_sCookieName,pCookieInfo);
	}
}

void CPpHttp::setHeader(const tstring& name, const tstring& value)
{
	if (!name.empty())
	{
		//m_pResHeaders.remove(name);
		m_pResHeaders.insert(name,CGC_VALUEINFO(value));
		//m_pResHeaders.push_back(CGC_KEYVALUE(name,CGC_VALUEINFO(value)));
	}
}
//void CPpHttp::setHeader(const tstring & header)
//{
//	if (!header.empty())
//	{
//		m_pResHeaders.push_back(header);
//	}
//}

void CPpHttp::forward(const tstring& url)
{
	m_forwardFromURL = m_requestURL;
	reset();

	//m_requestURL = url.empty() ? "/" : url;
	const tstring::size_type find = m_requestURL.rfind("/");
	if (url.empty())
		m_requestURL = "/";
	else if (find==tstring::npos || url.substr(0,1)=="/" || url.substr(0,3)=="www" || url.substr(0,4)=="http")
		m_requestURL = url;
	else
	{
		std::string sTemp = m_requestURL.substr(0,find+1);
		m_requestURL = sTemp+url;
	}
	//printf("******* m_forwardFromURL=%s\n",m_forwardFromURL.c_str());
	//printf("******* forward(): =%s\n",m_requestURL.c_str());
	GeServletInfo();
	GeRequestInfo();
}
void CPpHttp::location(const tstring& url)
{
	// /eb/login.html?type=config/config.csp
	//printf("******* location(): url=%s\n",url.c_str());
	const tstring::size_type find = m_requestURL.rfind("/");
	if (url.empty())
		m_sLocation = "/";
	else if (find==tstring::npos || url.substr(0,1)=="/" || url.substr(0,3)=="www" || url.substr(0,4)=="http")
		m_sLocation = url;
	else
	{
		std::string sTemp = m_requestURL.substr(0,find+1);
		m_sLocation = sTemp+url;
	}
	//printf("******* location(): =%s\n",m_sLocation.c_str());
}

const char * CPpHttp::getHttpResult(size_t& outSize) const
{
	// Make Response
	if (m_pHeaderBufferTemp==NULL)
		const_cast<CPpHttp*>(this)->m_pHeaderBufferTemp = new char[MAX_HTTPHEAD_SIZE];
	if (m_pHeaderTemp==NULL)
		const_cast<CPpHttp*>(this)->m_pHeaderTemp = new char[1024*4];
	if (m_pCookieTemp==NULL)
		const_cast<CPpHttp*>(this)->m_pCookieTemp = new char[1024*4];
	// get all headers
	std::string sHeaders;
	{
		CLockMap<tstring,cgcValueInfo::pointer>::const_iterator pIter = m_pResHeaders.begin();
		for (;pIter!=m_pResHeaders.end();pIter++)
		{
			const tstring sKey = pIter->first;
			const cgcValueInfo::pointer pValue = pIter->second;
			sprintf(m_pHeaderTemp,"%s: %s\r\n",sKey.c_str(),pValue->getStr().c_str());
			sHeaders.append(m_pHeaderTemp);
		}
		//for (size_t i=0;i<m_pResHeaders.size();i++)
		//{
		//	const cgcKeyValue::pointer pHeader = m_pResHeaders[i];
		//	sprintf(lpszHeader,"%s: %s\r\n",pHeader->getKey().c_str(),pHeader->getValue()->getStr().c_str());
		//	sHeaders.append(lpszHeader);
		//}
	}
	// Cookies
	{
		CLockMap<tstring,cgcCookieInfo::pointer>::const_iterator pIter = m_pResCookies.begin();
		for (;pIter!=m_pResCookies.end();pIter++)
		{
			const tstring sKey = pIter->first;
			const cgcCookieInfo::pointer pCookieInfo = pIter->second;
			//const cgcValueInfo::pointer pValue = pIter->second;
			sprintf(m_pCookieTemp,"%s=%s; path=%s",sKey.c_str(),pCookieInfo->m_sCookieValue.c_str(),pCookieInfo->m_sCookiePath.c_str());
			if (pCookieInfo->m_tExpiresTime > 0)
			{
				// 设置cookie过期时间
				struct tm *newtime;
				time_t ltime;
				time(&ltime);
				ltime += pCookieInfo->m_tExpiresTime*60;
				newtime = gmtime(&ltime);
				char szDT[128];
				strftime(szDT, 128, "%a, %d %b %Y %H:%M:%S GMT", newtime);
				sprintf(m_pHeaderTemp, "Set-Cookie: %s; Expires=%s\r\n", m_pCookieTemp,szDT);
			}else
			{
				sprintf(m_pHeaderTemp, "Set-Cookie: %s\r\n", m_pCookieTemp);
			}
			sHeaders.append(m_pHeaderTemp);
		}

		//for (size_t i=0;i<m_pResCookies.size();i++)
		//{
		//	const cgcKeyValue::pointer pCookie = m_pResCookies[i];
		//	sprintf(lpszCookie,"%s=%s",pCookie->getKey().c_str(),pCookie->getValue()->getStr().c_str());
		//	if (m_nCookieExpiresMinute > 0)
		//	{
		//		// 设置cookie过期时间
		//		struct tm *newtime;
		//		time_t ltime;
		//		time(&ltime);
		//		ltime += m_nCookieExpiresMinute*60;
		//		newtime = gmtime(&ltime);
		//		char szDT[128];
		//		strftime(szDT, 128, "%a, %d %b %Y %H:%M:%S GMT", newtime);
		//		sprintf(m_pHeaderTemp, "Set-Cookie: %s; Expires=%s\r\n", lpszCookie,szDT);
		//	}else
		//	{
		//		sprintf(m_pHeaderTemp, "Set-Cookie: %s\r\n", lpszCookie);
		//	}
		//	sHeaders.append(m_pHeaderTemp);
		//}
	}
	// Date: xxx
	if (m_addDateHeader)
	{
		// Obtain current GMT date/time
		struct tm *newtime;
		time_t ltime;
		time(&ltime);
		newtime = gmtime(&ltime);
		char szDT[128];
		strftime(szDT, 128, "%a, %d %b %Y %H:%M:%S GMT", newtime);
		sprintf(m_pHeaderTemp, "Date: %s\r\n", szDT);
		sHeaders.append(m_pHeaderTemp);
	}
	// Location: xxx
	if (!m_sLocation.empty())
	{
		sprintf(m_pHeaderTemp, "Location: %s\r\n", m_sLocation.c_str());
		sHeaders.append(m_pHeaderTemp);
	}
	// WWW-Authenticate: xxx
	if (m_statusCode == STATUS_CODE_401)
	{
		sprintf(m_pHeaderTemp, "WWW-Authenticate: Basic realm=\"%s\"\r\n", m_host.c_str());
		sHeaders.append(m_pHeaderTemp);
	}
	// Connection: xxx
	if (m_keepAlive && m_keepAliveInterval>=0)
	{
		sprintf(m_pHeaderTemp, "Connection: Keep-Alive\r\n");
		//sprintf(m_pHeaderTemp, "Connection: Keep-Alive\r\nKeep-Alive: 300\r\n");
	}else
	{
		sprintf(m_pHeaderTemp, "Connection: close\r\n");
	}
	sHeaders.append(m_pHeaderTemp);
	// Server: xxx
	sprintf(m_pHeaderTemp,"Server: %s\r\n",SERVERNAME);
	sHeaders.append(m_pHeaderTemp);
	// Content-Length: xxx
	if (m_addContentLength)
	{
		sprintf(m_pHeaderTemp,"Content-Length: %d\r\n",m_bodySize);
		sHeaders.append(m_pHeaderTemp);
	}

	// Transfer-Encoding: chunked\r\n 一直当接收不完整，会空白
	// Accept-Ranges: bytes\r\n
	sprintf(m_pHeaderBufferTemp, "HTTP/1.1 %s\r\n%sContent-Type: %s\r\n\r\n",
		cgcGetStatusCode(m_statusCode).c_str(), sHeaders.c_str(), m_sResContentType.c_str());

	const size_t headerSize = strlen(m_pHeaderBufferTemp);
	outSize = m_bodySize + headerSize;
	memcpy(m_resultBuffer, m_pHeaderBufferTemp, headerSize);
	memmove(m_resultBuffer+headerSize, m_resultBuffer+MAX_HTTPHEAD_SIZE, m_bodySize);
	//memcpy(m_resultBuffer+headerSize, m_resultBuffer+MAX_HTTPHEAD_SIZE, m_bodySize);
	m_resultBuffer[outSize] = '\0';
	return m_resultBuffer;
}

bool CPpHttp::doParse(const unsigned char * requestData, size_t requestSize,const char* sEncoding)
{
	if (requestData == NULL) return false;

	bool bGetHeader = false;
	bool ret = IsComplete((const char*)requestData,requestSize,bGetHeader);
	if (bGetHeader)
	{
		const tstring sConnection = getHeader(Http_Connection, "");
		m_keepAlive = sConnection == "Keep-Alive" || sConnection == "keep-alive";
		tstring sKeepAlive = getHeader(Http_KeepAlive, "0");
		m_keepAliveInterval = atoi(sKeepAlive.c_str());
		m_sReqContentType = getHeader(Http_ContentType, "");
		m_host = getHeader(Http_Host, "");
		m_sMyCookieSessionId = getCookie(Http_CookieSessionId, "");
		const tstring sRange = getHeader(Http_Range, "");
		if (!sRange.empty())
		{
			//Range: bytes=500-      表示读取该文件的500-999字节，共500字节。
			//Range: bytes=500-599   表示读取该文件的500-599字节，共100字节。
			tstring::size_type find = sRange.find("bytes=");
			if (find != tstring::npos)
			{
				m_nRangeFrom = atoi(sRange.substr(find+6).c_str());
				find = sRange.find("-",7);
				if (find != tstring::npos)
					m_nRangeTo = atoi(sRange.substr(find+1).c_str());
			}
		}
		tstring authorization = getHeader(Http_Authorization, "");
		if (!authorization.empty())
		{
			if (authorization.substr(0, 6) == "Basic ")
			{
				char * buffer = new char[int(authorization.size()*0.8)];
				long len = Base64Decode((unsigned char*)buffer, authorization.c_str()+6);
				if (len > 0)
				{
					authorization = std::string(buffer, len);
					tstring::size_type find = authorization.find(":");
					if (find != tstring::npos)
					{
						m_account = authorization.substr(0, find);
						m_secure = authorization.substr(find+1);
					}
				}
				delete[] buffer;
			}
		}
	}
	if (ret)
	{
		GeRequestInfo();
	}
	return ret;
}

void CPpHttp::GeServletInfo(void)
{
	short nfindsize = 6;	// "/rest."
	tstring::size_type findServlet1 = m_requestURL.find("/rest.");
	if (findServlet1==tstring::npos)
	{
		nfindsize = 9;		// "/servlet."
		findServlet1 = m_requestURL.find("/servlet.");
	}
	if (findServlet1 != tstring::npos)
	{
		tstring::size_type findServlet2 = 0;
		if (nfindsize == 6)
		{
			// 支持版本号
			// 格式：/rest.vvv.module.func?
			findServlet2 = m_requestURL.find(".", nfindsize+1);
			if (findServlet2 != tstring::npos)
			{
				m_restVersion = m_requestURL.substr(nfindsize,findServlet2-nfindsize);
				nfindsize = findServlet2+1;	// **必须放后面
			}
		}
		findServlet2 = m_requestURL.find("?", nfindsize+1);
		if (findServlet2 == tstring::npos)
			m_moduleName = m_requestURL.substr(nfindsize, m_requestURL.size()-findServlet1-nfindsize);
		else
			m_moduleName = m_requestURL.substr(nfindsize, findServlet2-findServlet1-nfindsize);

		findServlet2 = m_moduleName.find(".");
		if (findServlet2 != tstring::npos)
		{
			m_functionName = "do";
			m_functionName.append(m_moduleName.substr(findServlet2+1, m_moduleName.size()-findServlet2-1));
			m_moduleName = m_moduleName.substr(0, findServlet2);
		}
		//printf("============%s,%s,%s=============\n",m_restVersion.c_str(),m_moduleName.c_str(),m_functionName.c_str());
	}
}

void CPpHttp::GetPropertys(const std::string& sString)
{
	if (sString.empty()) return;
	tstring parameter;
	std::string::size_type find = 0;
	do
	{
		// Get [parameter=value]
		tstring::size_type findParameter = sString.find("&", find+1);
		if (findParameter == std::string::npos)
		{
			parameter = sString.substr(find, sString.size()-find);
		}else
		{
			parameter = sString.substr(find, findParameter-find);
			findParameter += 1;
		}
		find = findParameter;

		// Get parameter/value
		findParameter = parameter.find("=", 1);
		if (findParameter == std::string::npos)
		{
			// ERROR
			break;
		}

		tstring p = parameter.substr(0, findParameter);
		tstring v = parameter.substr(findParameter+1, parameter.size()-findParameter);

		m_propertys.setProperty(p, CGC_VALUEINFO(v));
	}while (find != std::string::npos);
}

void CPpHttp::GeRequestInfo(void)
{
	m_requestURI = m_requestURL;	
	//printf("******* GeRequestInfo(): m_requestURI=%s\n",m_requestURI.c_str());
	//if (m_method != HTTP_POST)
		//if (m_method == HTTP_GET)
	{
		std::string::size_type find = m_requestURL.find("?");
		if (find != std::string::npos)
		{
			m_requestURI = m_requestURL.substr(0, find);
			m_queryString = m_requestURL.substr(find+1);
			const tstring::size_type findFileName = m_requestURI.rfind("/");
			if (findFileName != std::string::npos)
			{
				m_fileName = m_requestURI.substr(findFileName+1);
			}
		}
	}

	std::string::size_type nFind = m_sReqContentType.find("application/x-www-form-urlencoded");
	if (nFind != std::string::npos)
	{
		m_queryString = URLDecode(m_queryString.c_str());
		if (!m_postString.empty())
			m_postString = URLDecode(m_postString.c_str());
	}

	GetPropertys(m_queryString);
	GetPropertys(m_postString);
	if (m_method == HTTP_POST || !m_postString.empty())
		m_queryString = m_postString;
	//if (!m_queryString.empty())
	//{
	//	tstring parameter;
	//	std::string::size_type find = 0;
	//	do
	//	{
	//		// Get [parameter=value]
	//		tstring::size_type findParameter = m_queryString.find("&", find+1);
	//		if (findParameter == std::string::npos)
	//		{
	//			parameter = m_queryString.substr(find, m_queryString.size()-find);
	//		}else
	//		{
	//			parameter = m_queryString.substr(find, findParameter-find);
	//			findParameter += 1;
	//		}
	//		find = findParameter;

	//		// Get parameter/value
	//		findParameter = parameter.find("=", 1);
	//		if (findParameter == std::string::npos)
	//		{
	//			// ERROR
	//			break;
	//		}

	//		tstring p = parameter.substr(0, findParameter);
	//		tstring v = parameter.substr(findParameter+1, parameter.size()-findParameter);

	//		m_propertys.setProperty(p, CGC_VALUEINFO(v));
	//	}while (find != std::string::npos);
	//}
}

bool CPpHttp::IsComplete(const char * httpRequest, size_t requestSize,bool& pOutHeader)
{
	tstring multipartyBoundary = "";
	const char * httpRequestOld = httpRequest;
	m_contentLength = requestSize;

	//printf("CPpHttp::IsComplete  size=%d\n",requestSize);

	// Check HTTP Method
	int leftIndex = 0;
	if (sotpCompare(httpRequest, "GET", leftIndex))
	{
		leftIndex += 4;
		m_method = HTTP_GET;
		m_functionName = "doGET";
		m_currentMultiPart.reset();
		m_sCurrentParameterData.clear();
	}else if (sotpCompare(httpRequest, "HEAD", leftIndex))
	{
		leftIndex += 5;
		m_method = HTTP_HEAD;
		m_functionName = "doHEAD";
		m_currentMultiPart.reset();
		m_sCurrentParameterData.clear();
	}else if (sotpCompare(httpRequest, "POST", leftIndex))
	{
		leftIndex += 5;
		m_method = HTTP_POST;
		m_functionName = "doPOST";
		m_currentMultiPart.reset();
		m_sCurrentParameterData.clear();
	}else if (sotpCompare(httpRequest, "PUT", leftIndex))
	{
		leftIndex += 4;
		m_method = HTTP_PUT;
		m_functionName = "doPUT";
		m_currentMultiPart.reset();
		m_sCurrentParameterData.clear();
	}else if (sotpCompare(httpRequest, "DELETE", leftIndex))
	{
		leftIndex += 7;
		m_method = HTTP_DELETE;
		m_functionName = "doDELETE";
		m_currentMultiPart.reset();
		m_sCurrentParameterData.clear();
	}else if (sotpCompare(httpRequest, "OPTIONS", leftIndex))
	{
		leftIndex += 8;
		m_method = HTTP_OPTIONS;
		m_functionName = "doOPTIONS";
		m_currentMultiPart.reset();
		m_sCurrentParameterData.clear();
	}else if (sotpCompare(httpRequest, "TRACE", leftIndex))
	{
		leftIndex += 6;
		m_method = HTTP_TRACE;
		m_functionName = "doTRACE";
		m_sCurrentParameterData.clear();
	}else if (sotpCompare(httpRequest, "CONNECT", leftIndex))
	{
		leftIndex += 8;
		m_method = HTTP_CONNECT;
		m_functionName = "doCONNECT";
		m_currentMultiPart.reset();
		m_sCurrentParameterData.clear();
	}else
	{
		if (m_currentMultiPart.get() == NULL) return false;
		/*
		// getMaxFileSize
		if (theUpload.getMaxFileSize() > 0 &&
		(int)(m_currentMultiPart->getUploadFile()->getFileSize() / 1024) > theUpload.getMaxFileSize())
		{
		return false;
		}
		*/
		//printf("**** m_contentSize=%d,%d\n",m_contentSize,m_receiveSize+requestSize);
		if (m_contentSize == requestSize && m_currentMultiPart->getBoundary().empty())
		{
			m_currentMultiPart.reset();
			m_queryString = httpRequest;
			m_postString = m_queryString;
			m_receiveSize = requestSize;
			return true;
		}else if (m_contentSize >= (m_receiveSize + requestSize) && m_currentMultiPart->getBoundary().empty())
		{
			//strncpy(m_contentData+m_receiveSize,httpRequest,requestSize);
			m_queryString.append(httpRequest);
			m_postString.append(httpRequest);
			m_receiveSize += requestSize;
			//if (m_receiveSize==m_contentSize)
			{
				m_currentMultiPart.reset();
				return true;
			}
			return false;
		}

		bool findBoundary = false;
		if (!m_sCurrentParameterData.empty() && m_currentMultiPart->getFileName().empty() && m_currentMultiPart->getName().empty() && !m_currentMultiPart->getBoundary().empty())
		{
			// 普通参数，前面有处理未完成数据；
			m_queryString.append(httpRequest);
			m_postString.append(httpRequest);
			m_receiveSize += requestSize;
			findBoundary = true;
			multipartyBoundary = m_currentMultiPart->getBoundary();
			m_sCurrentParameterData.append(httpRequest,requestSize);
			//if (sotpCompare(m_sCurrentParameterData.c_str(), Http_ContentDisposition.c_str(), leftIndex))
			{
				httpRequest = m_sCurrentParameterData.c_str();
				requestSize = m_sCurrentParameterData.size();
			}
		}else if (m_currentMultiPart->getFileName().empty() ||				// 普通参数，不是文件
			requestSize >= m_currentMultiPart->getBoundary().size()+2)		// 文件
		{
			if (sotpCompare(httpRequest, m_currentMultiPart->getBoundary().c_str(), leftIndex))
			{
				findBoundary = true;
				multipartyBoundary = m_currentMultiPart->getBoundary();
				httpRequest += multipartyBoundary.size() + 2;
			}else
			{
				tstring boundaryEnd("\r\n");
				boundaryEnd.append(m_currentMultiPart->getBoundary());
				const char * find = strstrl(httpRequest, boundaryEnd.c_str(), requestSize, boundaryEnd.size());
				if (find != NULL)
				{
					findBoundary = true;
					if (m_currentMultiPart->getUploadFile()->getFileSize() > 0)
					{
						m_currentMultiPart->write((const char*)httpRequest, find-(const char*)httpRequest);
						m_currentMultiPart->close();
						m_currentMultiPart->setParser(cgcNullParserBaseService);
						m_files.push_back(m_currentMultiPart);
						multipartyBoundary = m_currentMultiPart->getBoundary();
						m_currentMultiPart.reset();

						// getMaxFileCount
						if (theUpload.getMaxFileCount() > 0 && (int)m_files.size() >= theUpload.getMaxFileCount())
						{
							return true;
						}
					}

					if (sotpCompare(find+boundaryEnd.size(), "--\r\n", leftIndex))
					{
						return true;
					}

					httpRequest = find;
				}
			}
		}else
		{
			m_currentMultiPart->close();
			m_currentMultiPart->setParser(cgcNullParserBaseService);
			m_files.push_back(m_currentMultiPart);
			m_currentMultiPart.reset();
			return true;
		}

		if (!findBoundary)
		{
			m_currentMultiPart->write((const char*)httpRequest, requestSize);
			// getMaxFileSize & isGreaterMaxSize
			if ((theUpload.getMaxFileSize() > 0 && (int)(m_currentMultiPart->getUploadFile()->getFileSize() / 1024) > theUpload.getMaxFileSize())
				|| isGreaterMaxSize())
			{
				m_statusCode = STATUS_CODE_413;
				m_currentMultiPart->close();
				m_currentMultiPart->setParser(cgcNullParserBaseService);
				m_files.push_back(m_currentMultiPart);
				m_currentMultiPart.reset();
				return true;	// 由api去处理
			}
			return false;
		}
	}

	const char * findSearch = NULL;
	const char * findSearchEnd = NULL;
	if (multipartyBoundary.empty())
	{
		// Get file name
		findSearch = strstr(httpRequest+leftIndex, " ");
		if (findSearch == NULL)
			return false;
		m_requestURL = tstring(httpRequest+leftIndex, findSearch-httpRequest-leftIndex);
		//printf("******* IsComplete(): Get file name;%s,%s\n",httpRequest,m_requestURL.c_str());
		m_requestURL = URLDecode(m_requestURL.c_str());
		leftIndex += (m_requestURL.size()+1);

		GeServletInfo();
		findSearchEnd = strstr(findSearch, "\r\n");
		if (findSearchEnd == NULL) return false;

		m_httpVersion = tstring(findSearch+1, findSearchEnd-findSearch-1);
		httpRequest = findSearchEnd + 2;
	}
	while (httpRequest != NULL)
	{
		if (!multipartyBoundary.empty())
		{
			tstring boundaryFind("\r\n");
			boundaryFind.append(multipartyBoundary);
			if (m_currentMultiPart.get() == NULL)
			{
				if (sotpCompare(httpRequest, boundaryFind.c_str(), leftIndex))
				{
					if (sotpCompare(httpRequest+boundaryFind.size()+leftIndex, "--\r\n", leftIndex))
					{
						// 最后完成1；
						//printf("******* final ok1 ************\n");
						return true;
					}
					httpRequest += boundaryFind.size() + 2;
					m_currentMultiPart = CGC_MULTIPART(multipartyBoundary);
					continue;
				}
			}else if (m_currentMultiPart->getFileName().empty())
			{
				// 不是文件参数，普通参数；
				//printf("***** httpRequest1=%s\n",httpRequest);
				if (sotpCompare(httpRequest, "\r\n", leftIndex))
				{
					httpRequest += (leftIndex + 2);
					//printf("***** httpRequest2=%s\n",httpRequest);
					findSearchEnd = strstr(httpRequest, boundaryFind.c_str());
					if (findSearchEnd == NULL) break;
					// 查找到一个参数；
					const tstring p = m_currentMultiPart->getName();
					const tstring v(httpRequest,findSearchEnd-httpRequest);
					//printf("**** %s:%s\n",p.c_str(),v.c_str());
					m_propertys.setProperty(p, CGC_VALUEINFO(v));
					m_currentMultiPart->close();
					m_currentMultiPart->setParser(cgcNullParserBaseService);
					m_currentMultiPart.reset();
					//httpRequest = findSearchEnd;	// ***不处理下面代码，为了保持可读性；-2是前面
					//continue;
					// ***直接使用下面代码，效率更高；
					httpRequest = findSearchEnd+boundaryFind.size();
					if (sotpCompare(httpRequest, "--\r\n", leftIndex))
					{
						// 最后完成2；
						//printf("******* final ok2 ************\n");
						return true;
					}
					httpRequest += 2;
					m_currentMultiPart = CGC_MULTIPART(multipartyBoundary);
				}
			}
		}
		findSearch = strstr(httpRequest, ":");
		if (findSearch == NULL) break;

		findSearchEnd = strstr(findSearch+1, "\r\n");
		if (findSearchEnd == NULL) break;

		pOutHeader = true;
		const tstring param(httpRequest, findSearch-httpRequest);
		const short nOffset = findSearch[1]==' '?2:1;	// 带空格2，不带空格1
		tstring value(findSearch+nOffset, findSearchEnd-findSearch-nOffset);
		if (param != Http_ContentDisposition)
			m_pReqHeaders.setProperty(param, CGC_VALUEINFO(value));
		//printf("IsComplete: %s: %s\n",param.c_str(),value.c_str());

		if (m_currentMultiPart.get() != NULL && param == Http_ContentDisposition)
		{
			bool doContinue = false;
			int leftIndexTemp = 0;
			while (!value.empty())
			{
				tstring valuetemp;
				tstring::size_type find = value.find(";");
				if (find == tstring::npos)
				{
					valuetemp = value;
					value.clear();
				}else
				{
					valuetemp = value.substr(0, find);
					value = value.substr(find+1);
				}

				if (sotpCompare(valuetemp.c_str(), "form-data;", leftIndexTemp))
				{
					// ?
					continue;
				}else if (sotpCompare(valuetemp.c_str(), "name=\"", leftIndexTemp))
				{
					m_currentMultiPart->setName(valuetemp.substr(leftIndexTemp+6, valuetemp.size()-leftIndexTemp-7));
					//printf("name=%s\n",m_currentMultiPart->getName().c_str());
				}else if (sotpCompare(valuetemp.c_str(), "filename=\"", leftIndexTemp))
				{
					tstring filename(valuetemp.substr(leftIndexTemp+10, valuetemp.size()-leftIndexTemp-11));
					find = filename.rfind("\\");
					if (find == tstring::npos)
						find = filename.rfind("/");
					if (find != tstring::npos)
						filename = filename.substr(find+1);
					m_currentMultiPart->setFileName(filename);
					if (m_currentMultiPart->getUploadFile()->getFileName().empty())
					{
						m_currentMultiPart->close();
						m_currentMultiPart->setParser(cgcNullParserBaseService);
						m_currentMultiPart.reset();
						tstring boundaryFind("\r\n");
						boundaryFind.append(multipartyBoundary);
						httpRequest = strstrl(httpRequest, boundaryFind.c_str(), requestSize-(httpRequest-httpRequestOld),boundaryFind.size());
						doContinue = true;
						break;
					}
				}
			}

			if (doContinue)
				continue;
		}else if (param == Http_ContentType)
		{
			int leftIndexTemp = 0;
			if (sotpCompare(value.c_str(), "multipart/form-data;", leftIndexTemp))
			{
				// isEnableUpload
				if (!theUpload.isEnableUpload())
					return false;

				value = value.substr(leftIndexTemp+20);
				if (sotpCompare(value.c_str(), "boundary=", leftIndexTemp))
				{
					multipartyBoundary = "--";
					multipartyBoundary.append(value.substr(leftIndexTemp+9));
					if (m_currentMultiPart.get() != NULL)
					{
						m_currentMultiPart->close();
						m_currentMultiPart->setParser(cgcNullParserBaseService);
						m_currentMultiPart.reset();
					}
				}
			}else if (m_currentMultiPart.get() != NULL && !m_currentMultiPart->getBoundary().empty())
			{
				// isEnableContentType
				if (!theUpload.isEnableContentType(value))
				{
					//printf("**** disable content type: %s\n",value.c_str());
					m_currentMultiPart->close();
					m_currentMultiPart->setParser(cgcNullParserBaseService);
					m_currentMultiPart.reset();
					return false;
				}
				m_currentMultiPart->setContentType(value);

				static tstring theTempSavePath;
				if (theTempSavePath.empty())
				{
					char tempSavePath[256];
					//tstring::size_type findpath = theUpload.getTempPath().find("/");
					//if (findpath == tstring::npos)
					//	findpath = theUpload.getTempPath().find("\\");
					//if (findpath == tstring::npos)
					{
						//sprintf(tempSavePath, "%s/%s", theApplication->getAppConfPath().c_str(), theUpload.getTempPath().c_str());
#ifdef WIN32
						if (theUpload.getTempPath().substr(1,1)==":")
							sprintf(tempSavePath, "%s", theUpload.getTempPath().c_str());
						else
							sprintf(tempSavePath, "c:\\%s", theUpload.getTempPath().c_str());
#else
						if (theUpload.getTempPath().substr(0,1)=="/")
							sprintf(tempSavePath, "%s", theUpload.getTempPath().c_str());
						else
							sprintf(tempSavePath, "/%s", theUpload.getTempPath().c_str());
#endif
						theTempSavePath = tempSavePath;
						if (m_fileSystemService.get() != NULL)
						{
							cgcValueInfo::pointer pOut = CGC_VALUEINFO(true);
							m_fileSystemService->callService("exists", CGC_VALUEINFO(theTempSavePath),pOut);
							if (!pOut->getBoolean())
							{
								m_fileSystemService->callService("create_directory", CGC_VALUEINFO(theTempSavePath));
							}
						}
						//}else
						//{
						//	sprintf(tempSavePath, "%s", theUpload.getTempPath().c_str());
					}
					theTempSavePath = tempSavePath;
				}
				m_currentMultiPart->open(theTempSavePath);

				tstring boundaryEnd("\r\n");
				boundaryEnd.append(m_currentMultiPart->getBoundary());
				const char * find = strstrl((const char*)findSearchEnd, boundaryEnd.c_str(), requestSize-(findSearchEnd-httpRequestOld), boundaryEnd.size());
				if (find == NULL)
				{
					m_currentMultiPart->write(findSearchEnd+4, m_contentLength-size_t(findSearchEnd-httpRequestOld)-4);
					return false;
				}

				m_currentMultiPart->write(findSearchEnd+4, find-findSearchEnd-4);
				m_currentMultiPart->close();
				m_currentMultiPart->setParser(cgcNullParserBaseService);
				m_files.push_back(m_currentMultiPart);

				// getMaxFileSize
				if (theUpload.getMaxFileSize() > 0 &&
					(int)(m_currentMultiPart->getUploadFile()->getFileSize() / 1024) > theUpload.getMaxFileSize())
				{
					m_statusCode = STATUS_CODE_413;
					m_currentMultiPart.reset();
					return true;	// 由api去处理
				}

				m_currentMultiPart.reset();
				// getMaxFileCount
				if (theUpload.getMaxFileCount() > 0 && (int)m_files.size() >= theUpload.getMaxFileCount())
				{
					return true;
				}

				httpRequest = find;
				continue;
			}
		}else if (param == Http_ContentLength)
		{
			m_contentSize = atoi(value.c_str());
			//printf("IsComplete: m_contentSize=%d\n",m_contentSize);
			if (m_contentSize > 0)
			{
				if ((theUpload.getMaxFileSize() > 0 && (int)(m_contentSize / 1024) > theUpload.getMaxFileSize())
					|| isGreaterMaxSize())
				{
					m_statusCode = STATUS_CODE_413;
					if (m_currentMultiPart.get()!=NULL)
					{
						m_currentMultiPart->close();
						m_currentMultiPart->setParser(cgcNullParserBaseService);
						m_files.push_back(m_currentMultiPart);
						m_currentMultiPart.reset();
					}
					return true;	// 由api去处理；
				}

				//if (multipartyBoundary.empty())
				{
					const char * find = strstrl(httpRequest, "\r\n\r\n", requestSize-(httpRequest-httpRequestOld), 4);
					//const char * find = strstrl(httpRequest, "\r\n\r\n", strlen(httpRequest), 4);
					if (find == NULL)
					{
						m_receiveSize = 0;
					}else
					{
						find += 4;
						m_receiveSize = requestSize-(int)(find-httpRequestOld);
					}
					//m_receiveSize = strlen(find);
					//printf("**** m_contentSize=%d,m_receiveSize=%d\n",m_contentSize,m_receiveSize);

					if (multipartyBoundary.empty())
					{
						if (m_receiveSize == 0)
						{
							m_currentMultiPart = CGC_MULTIPART("");
							//return false;
						}else
						{
							//int offset = 0;
							//if (httpRequest[strlen(httpRequest)-1] == '\n')
							//	offset += 1;
							//if (httpRequest[strlen(httpRequest)-2] == '\r')
							//	offset += 1;

							//if (m_contentData)
							//	delete[] m_contentData;
							//m_contentData = new char[m_contentSize+1];
							//strncpy(m_contentData, find, m_receiveSize);
							////strncpy(m_contentData, httpRequest+strlen(httpRequest)-m_contentSize-offset, m_contentSize);
							//m_contentData[m_receiveSize] = '\0';
							////printf("=================\n%s\n================\n",m_contentData);
							//m_queryString = m_contentData;
							m_queryString = find;
							m_postString = find;
							//m_queryString = tstring(m_contentData, m_contentSize);
							if (m_contentSize > m_receiveSize)
							{
								m_currentMultiPart = CGC_MULTIPART("");
								// 下面会返回false
							}
						}
					}
				}
			}
		}else if (param == Http_Cookie)
		{
			while (!value.empty())
			{
				// 去掉头尾空格
				value.erase(0,value.find_first_not_of(" "));
				value.erase(value.find_last_not_of(" ") + 1);
				std::string::size_type find1 = value.find("=");
				if (find1 == std::string::npos) break;

				std::string::size_type find2 = value.find(";",find1+1);
				if (find2 == std::string::npos)
				{
					tstring cookieName = value.substr(0,find1);
					tstring cookieValue = value.substr(find1+1);
					m_pReqCookies.setProperty(cookieName, CGC_VALUEINFO(cookieValue));
					break;
				}
				tstring cookieName = value.substr(0,find1);
				tstring cookieValue = value.substr(find1+1,find2-find1-1);
				m_pReqCookies.setProperty(cookieName, CGC_VALUEINFO(cookieValue));
				value = value.substr(find2+1);
			}
		}
		httpRequest = findSearchEnd + 2;
	}

	if (!multipartyBoundary.empty() && m_currentMultiPart.get() == NULL)
	{
		m_currentMultiPart = CGC_MULTIPART(multipartyBoundary);
		return false;
	}else if (m_currentMultiPart.get() != NULL)
	//}else if (m_currentMultiPart.get() != NULL && m_currentMultiPart->getBoundary().empty())
	{
		// 数据未收完整
		if (httpRequest!=NULL && m_currentMultiPart->getFileName().empty() && m_currentMultiPart->getName().empty() && !m_currentMultiPart->getBoundary().empty())
		{
			// 普通参数，不是文件，并且当前未解析到名称，说明协议被截包；
			// 记下当前参数内容，下次再解析多一次；预防截包	如：Content-Disposition: form-data; name="
			m_sCurrentParameterData = httpRequest;
		}

		return false;
	}

	return true;
}

const char * CPpHttp::strstrl(const char * sourceBuffer, const char * findBuffer, size_t sourceSize, size_t fineSize)
{
	if (sourceBuffer == NULL || findBuffer == NULL) return NULL;
	if (fineSize > sourceSize) return NULL;

	size_t sourceIndex = 0;
	size_t findIndex = 0;
	while (sourceIndex < sourceSize)
	{
		if ((char)sourceBuffer[sourceIndex] != (char)findBuffer[findIndex])
		{
			if (findIndex == 0)
				sourceIndex++;
			else
				findIndex = 0;
			continue;
		}
		sourceIndex++;
		if (++findIndex == fineSize)
			return sourceBuffer + sourceIndex - findIndex;
	}

	return NULL;
}

bool CPpHttp::sotpCompare(const char * pBuffer, const char * pCompare, int & leftIndex)
{
	int i1 = 0, i2 = 0;
	leftIndex = 0;
	// 判断前面空格或者‘TAB’键；
	while (' ' == pBuffer[leftIndex] || '\t' == pBuffer[leftIndex])
	{
		leftIndex++;
	}

	i1 = leftIndex;
	while (pCompare[i2] != '\0')
	{
		if (pCompare[i2++] != pBuffer[i1] || '\0' == pBuffer[i1])
		{
			return false;
		}
		i1++;
	}
	return true;
}

bool CPpHttp::isGreaterMaxSize(void) const
{
	if (theUpload.getMaxUploadSize() <= 0) return false;

	int uploadSizes = 0;
	for (size_t i=0; i<m_files.size(); i++)
	{
		uploadSizes += m_files[i]->getUploadFile()->getFileSize();
		if (uploadSizes > theUpload.getMaxUploadSize())
			return true;
	}
	return false;
}
unsigned char CPpHttp::toHex(const unsigned char &x)
{
	return x > 9 ? x -10 + 'A': x + '0';
}

unsigned char CPpHttp::fromHex(const unsigned char &x)
{
	return isdigit(x) ? x-'0' : x-'A'+10;
}

std::string CPpHttp::URLEncode(const char *sIn)
{
	const size_t nLen = strlen(sIn);
	std::string sOut;
	for( size_t ix = 0; ix < nLen; ix++ )
	{
		unsigned char buf[4];
		memset( buf, 0, 4 );
		if( isalnum( (unsigned char)sIn[ix] ) )
		{      
			buf[0] = sIn[ix];
		}
		else
		{
			buf[0] = '%';
			buf[1] = toHex( (unsigned char)sIn[ix] >> 4 );
			buf[2] = toHex( (unsigned char)sIn[ix] % 16);
		}
		sOut += (char *)buf;
	}
	return sOut;
}

std::string CPpHttp::URLDecode(const char *sIn)
{
	const size_t nLen = strlen(sIn);
	std::string sOut;
	for( size_t ix = 0; ix < nLen; ix++ )
	{
		unsigned char ch = 0;
		if(sIn[ix]=='%')
		{
			ch = (fromHex(sIn[ix+1])<<4);
			ch |= fromHex(sIn[ix+2]);
			ix += 2;
		}
		else if(sIn[ix] == '+')
		{
			ch = ' ';
		}
		else
		{
			ch = sIn[ix];
		}
		sOut += (char)ch;
	}

	return sOut;

}

} // cgc namespace
