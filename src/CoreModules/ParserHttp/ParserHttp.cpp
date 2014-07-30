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
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
#endif // WIN32


//#include "Base64.h"
// cgc head
#include <CGCBase/app.h>
#include <CGCBase/cgcParserHttp.h>
using namespace cgc;
/*
#include "XmlParseUpload.h"

const size_t MAX_HTTPHEAD_SIZE		= 2*1024;
const size_t INCREASE_BODY_SIZE		= 20*1024;

Xmlconst char * SERVERNAME		= "MYCP Http Server/1.0";
ParseUpload theUpload;

class CParserHttp
	: public cgcParserHttp
{
private:
	virtual tstring serviceName(void) const {return _T("PARSERHTTP");}
	virtual void stopService(void) {}

	tstring m_host;
	tstring m_account;
	tstring m_secure;

	// 1 servlet	: "GET /servlet/ModuleName?p=v&... HTTP/1.1"
	// 2 servlet	: "GET /servlet/ModuleName/FunctionName?p=v&... HTTP/1.1"
	// 3 csp		: "GET /ModuleName/FunctionName.csp?p=v&... HTTP/1.1"
	tstring m_moduleName;		// 12 "ModuleName"; 3 ""
	tstring m_functionName;		// 1 "doGET"; 2 "FunctionName"; 3 ""
	tstring m_httpVersion;		// "HTTP/1.1"

	size_t m_contentLength;
	HTTP_METHOD m_method;		// GET
	tstring m_requestURL;		// "/path/file.csp?p=v&..."
	tstring m_requestURI;		// "/path/file.csp"
	tstring m_queryString;		// "p=v&..."
	tstring m_fileName;			// file.csp

	bool m_keepAlive;
	int m_keepAliveInterval;
	char * m_contentData;
	size_t m_contentSize;

	cgcAttributes::pointer m_headers;
	cgcAttributes::pointer m_propertys;

	// application/octet-stream

	// Content-Disposition: form-data; name="file"; filename="a.txt"

	// image/gif或者image/jpg

	//Content-Disposition: form-data; name="filename"; filename="D:\Temp\TestFile.txt"
	// Content-Type: text/plain
	// Content-Type属性没有的话表示普通的字符串数据，如"company"＝XX


	HTTP_STATUSCODE m_statusCode;
	bool m_addDateHeader;
	tstring m_sReqContentType;
	tstring m_sResContentType;
	tstring m_sLocation;

	char * m_resultBuffer;
	size_t m_bodySize;
	size_t m_bodyBufferSize;

	tstring m_forwardFromURL;

	std::vector<cgcMultiPart::pointer> m_files;
	cgcMultiPart::pointer m_currentMultiPart;

	cgcServiceInterface::pointer m_fileSystemService;

protected:
	//////////////////////////////////////////////////
	// Request
	const tstring& getHost(void) const {return m_host;}

	virtual const tstring& getModuleName(void) const {return m_moduleName;}
	virtual const tstring& getFunctionName(void) const {return m_functionName;}

	virtual const tstring& getHttpVersion(void) const {return m_httpVersion;}

	virtual HTTP_METHOD getHttpMethod(void) const {return m_method;}
	virtual bool isServletRequest(void) const {return !m_moduleName.empty() && !m_functionName.empty();}
	virtual size_t getContentLength(void) const {return m_contentLength;}
	virtual const tstring& getRequestURL(void) const {return m_requestURL;}
	virtual const tstring& getRequestURI(void) const {return m_requestURI;}
	virtual const tstring& getQueryString(void) const {return m_queryString;}
	virtual const tstring& getFileName(void) const {return m_fileName;}
	virtual bool isKeepAlive(void) const {return m_keepAlive;}
	virtual int getKeepAlive(void) const {return m_keepAliveInterval;}
	//virtual size_t getContentLength(void) const {return m_contentSize;}
	//virtual const char* getContentData(void) const {return m_contentData;}

	virtual cgcMultiPart::pointer getMultiPart(void) const{return m_currentMultiPart;}
	virtual bool getUploadFile(std::vector<cgcUploadFile::pointer>& outFiles) const
	{
		for (size_t i=0; i<m_files.size(); i++)
		{
			outFiles.push_back(m_files[i]->getUploadFile());
		}
		return !m_files.empty();
		//return m_currentMultiPart.get() == NULL ? cgcNullUploadFile : m_currentMultiPart->getUploadFile();
	}

	virtual tstring getHeader(const tstring & header, const tstring& defaultValue) const
	{
		cgcValueInfo::pointer valueInfo = m_headers->getProperty(header);
		return valueInfo.get() == NULL ? defaultValue : valueInfo->getStr();
	}
	virtual bool getHeaders(std::vector<cgcKeyValue::pointer>& outHeaders) const {return m_headers->getSPropertys(outHeaders);}

	// Parameter
	virtual cgcValueInfo::pointer getParameter(const tstring & paramName) const {return m_propertys->getProperty(paramName);}
	bool getParameter(const tstring & paramName, std::vector<cgcValueInfo::pointer>& outParameters) const {return m_propertys->getProperty(paramName, outParameters);}
	bool getParameters(std::vector<cgcKeyValue::pointer>& outParameters) const {return m_propertys->getSPropertys(outParameters);}

	/////////////////////////////////////////////
	// Response
	virtual void println(const char * text, size_t size)
	{
		write(text, size);
		write("\r\n", 2);
	}
	virtual void println(const tstring& text)
	{
		write(text.c_str(), text.size());
		write("\r\n", 2);
	}
	virtual void write(const char * text, size_t size)
	{
		if (text == NULL || size == std::string::npos) return;

		if (m_method == HTTP_HEAD) return;

		if (m_bodySize+size > m_bodyBufferSize)
		{
			char * bufferTemp = new char[m_bodySize+1];
			memcpy(bufferTemp, m_resultBuffer+MAX_HTTPHEAD_SIZE, m_bodySize);
			delete[] m_resultBuffer;

			m_bodyBufferSize += size > INCREASE_BODY_SIZE ? size : INCREASE_BODY_SIZE;
			m_resultBuffer = new char[MAX_HTTPHEAD_SIZE+m_bodyBufferSize+1];
			memcpy(m_resultBuffer+MAX_HTTPHEAD_SIZE, bufferTemp, m_bodySize);
			delete[] bufferTemp;
		}
		memcpy(m_resultBuffer+MAX_HTTPHEAD_SIZE+m_bodySize, text, size);
		m_bodySize += size;
	}
	virtual void write(const tstring& text)
	{
		write(text.c_str(), text.size());
	}
	virtual void newline(void)
	{
		write("\r\n", 2);
	}
	virtual void reset(void)
	{
		m_statusCode = STATUS_CODE_200;
		m_sResContentType = "text/html";
		m_sLocation = "";
		m_moduleName = "";
		m_functionName = "";
		m_addDateHeader = false;
		m_bodySize = 0;
		memset(m_resultBuffer, 0, m_bodyBufferSize);
	}

	virtual void setStatusCode(HTTP_STATUSCODE statusCode) {m_statusCode = statusCode;}
	virtual HTTP_STATUSCODE getStatusCode(void) const {return m_statusCode;}
	virtual void addDateHeader(void) {m_addDateHeader = true;}
	virtual size_t getBodySize(void) const {return m_bodySize;}
	virtual const tstring& getForwardFromURL(void) const {return m_forwardFromURL;}

	virtual void setContentType(const tstring& contentType) {m_sResContentType = contentType;}
	virtual const tstring& getContentType(void) const {return m_sResContentType;}
	virtual const tstring& getReqContentType(void) const {return m_sReqContentType;}

	virtual tstring getAccount(void) const {return m_account;}
	virtual tstring getSecure(void) const {return m_secure;}

	virtual void forward(const tstring& url)
	{
		m_forwardFromURL = m_requestURL;
		reset();

		m_requestURL = url.empty() ? "/" : url;
		GeServletInfo();
		GeRequestInfo();
	}
	virtual void location(const tstring& url) {m_sLocation = url;}

	virtual const char * getHttpResult(size_t& outSize) const
	{
		// Make Response
		char headerBuffer[MAX_HTTPHEAD_SIZE];
		char headerDate[128];
		memset(headerDate, 0, sizeof(headerDate));
		if (m_addDateHeader)
		{
			// Obtain current GMT date/time
			struct tm *newtime;
			time_t ltime;

			time(&ltime);
			newtime = gmtime(&ltime);
			char szDT[128];
			strftime(szDT, 128, "%a, %d %b %Y %H:%M:%S GMT", newtime);
			sprintf(headerDate, "Date: %s\r\n", szDT);
		}
		char locationBuffer[256];
		memset(locationBuffer, 0, sizeof(locationBuffer));
		if (!m_sLocation.empty())
		{
			sprintf(locationBuffer, "Location: %s\r\n", m_sLocation.c_str());
		}

		char authenticateBuffer[256];
		memset(authenticateBuffer, 0, sizeof(authenticateBuffer));
		if (m_statusCode == STATUS_CODE_401)
		{
			sprintf(authenticateBuffer, "WWW-Authenticate: Basic realm=\"%s\"\r\n", m_host.c_str());
		}
		sprintf(headerBuffer, "HTTP/1.1 %s\r\n%s%sServer: %s\r\n%sAccept-Ranges: bytes\r\nContent-Length: %d\r\nConnection: %s\r\nContent-Type: %s\r\n\r\n",
			cgcGetStatusCode(m_statusCode).c_str(), headerDate, locationBuffer, SERVERNAME, authenticateBuffer,
			m_bodySize, m_keepAlive ? "Keep-Alive" : "close", m_sResContentType.c_str());

		size_t headerSize = strlen(headerBuffer);
		outSize = m_bodySize + headerSize;
		memcpy(m_resultBuffer, headerBuffer, headerSize);
		// ???
		memmove(m_resultBuffer+headerSize, m_resultBuffer+MAX_HTTPHEAD_SIZE, m_bodySize);
		//memcpy(m_resultBuffer+headerSize, m_resultBuffer+MAX_HTTPHEAD_SIZE, m_bodySize);
		m_resultBuffer[outSize] = '\0';
		return m_resultBuffer;
	}

	virtual bool doParse(const unsigned char * requestData, size_t requestSize,const char* sEncoding)
	{
		if (requestData == NULL) return false;

		if (!IsComplete((const char*)requestData, requestSize))
		{
			return false;
		}

		tstring sConnection = getHeader(Http_Connection, "");
		m_keepAlive = sConnection == "Keep-Alive" || sConnection == "keep-alive";
		tstring sKeepAlive = getHeader(Http_KeepAlive, "0");
		m_keepAliveInterval = atoi(sKeepAlive.c_str());
		m_sReqContentType = getHeader(Http_ContentType, "");
		m_host = getHeader(Http_Host, "");
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
		

		GeRequestInfo();
		return true;
	}

	void GeServletInfo(void)
	{
		tstring::size_type findServlet1 = m_requestURL.find("/servlet.");
		//tstring::size_type findServlet1 = m_requestURL.find("/servlet/");
		if (findServlet1 != tstring::npos)
		{
			tstring::size_type findServlet2 = m_requestURL.find("?", 10);
			if (findServlet2 == tstring::npos)
				m_moduleName = m_requestURL.substr(9, m_requestURL.size()-findServlet1-9);
			else
				m_moduleName = m_requestURL.substr(9, findServlet2-findServlet1-9);

			findServlet2 = m_moduleName.find(".");
			//findServlet2 = m_moduleName.find("/");
			if (findServlet2 != tstring::npos)
			{
				m_functionName = "do";
				m_functionName.append(m_moduleName.substr(findServlet2+1, m_moduleName.size()-findServlet2-1));
				m_moduleName = m_moduleName.substr(0, findServlet2);
			}
		}
	}

	void GeRequestInfo(void)
	{
		m_requestURI = m_requestURL;		
		if (m_method != HTTP_POST)
		//if (m_method == HTTP_GET)
		{
			std::string::size_type find = m_requestURL.find("?");
			if (find != std::string::npos)
			{
				m_requestURI = m_requestURL.substr(0, find);
				m_queryString = m_requestURL.substr(find+1);
				tstring::size_type findFileName = m_requestURI.rfind("/");
				if (findFileName != std::string::npos)
				{
					m_fileName = m_requestURI.substr(findFileName+1);
				}
			}
		}

		if (m_sReqContentType == "application/x-www-form-urlencoded")
		{
			m_queryString = URLDecode(m_queryString.c_str());
		}

		if (!m_queryString.empty())
		{
			tstring parameter;
			std::string::size_type find = 0;
			do
			{
				// Get [parameter=value]
				tstring::size_type findParameter = m_queryString.find("&", find+1);
				if (findParameter == std::string::npos)
				{
					parameter = m_queryString.substr(find, m_queryString.size()-find);
				}else
				{
					parameter = m_queryString.substr(find, findParameter-find);
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

				m_propertys->setProperty(p, CGC_VALUEINFO(v));
			}while (find != std::string::npos);
		}
	}

	bool IsComplete(const char * httpRequest, size_t requestSize)
	{
		tstring multipartyBoundary = "";
		const char * httpRequestOld = httpRequest;
		m_contentLength = requestSize;

		// Check HTTP Method
		int leftIndex = 0;
		if (sotpCompare(httpRequest, "GET", leftIndex))
		{
			leftIndex += 4;
			m_method = HTTP_GET;
			m_functionName = "doGET";
		}else if (sotpCompare(httpRequest, "HEAD", leftIndex))
		{
			leftIndex += 5;
			m_method = HTTP_HEAD;
			m_functionName = "doHEAD";
		}else if (sotpCompare(httpRequest, "POST", leftIndex))
		{
			leftIndex += 5;
			m_method = HTTP_POST;
			m_functionName = "doPOST";
		}else if (sotpCompare(httpRequest, "PUT", leftIndex))
		{
			leftIndex += 4;
			m_method = HTTP_PUT;
			m_functionName = "doPUT";
		}else if (sotpCompare(httpRequest, "DELETE", leftIndex))
		{
			leftIndex += 7;
			m_method = HTTP_DELETE;
			m_functionName = "doDELETE";
		}else if (sotpCompare(httpRequest, "OPTIONS", leftIndex))
		{
			leftIndex += 8;
			m_method = HTTP_OPTIONS;
			m_functionName = "doOPTIONS";
		}else if (sotpCompare(httpRequest, "TRACE", leftIndex))
		{
			leftIndex += 6;
			m_method = HTTP_TRACE;
			m_functionName = "doTRACE";
		}else if (sotpCompare(httpRequest, "CONNECT", leftIndex))
		{
			leftIndex += 8;
			m_method = HTTP_CONNECT;
			m_functionName = "doCONNECT";
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
*//*
			if (m_contentSize == requestSize && m_currentMultiPart->getBoundary().empty())
			{
				m_currentMultiPart.reset();
				m_queryString = httpRequest;
				return true;
			}

			bool findBoundary = false;
			if (requestSize >= m_currentMultiPart->getBoundary().size()+2)
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
					m_currentMultiPart->close();
					m_currentMultiPart->setParser(cgcNullParserBaseService);
					m_files.push_back(m_currentMultiPart);
					m_currentMultiPart.reset();
					return false;
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
			if (!multipartyBoundary.empty() && m_currentMultiPart.get() == NULL)
			{
				tstring boundaryFind("\r\n");
				boundaryFind.append(multipartyBoundary);
				if (sotpCompare(httpRequest, boundaryFind.c_str(), leftIndex))
				{
					if (sotpCompare(httpRequest+boundaryFind.size()+leftIndex, "--\r\n", leftIndex))
					{
						return true;
					}
					httpRequest += boundaryFind.size() + 2;
					m_currentMultiPart = CGC_MULTIPART(multipartyBoundary);
					continue;
				}
			}
			findSearch = strstr(httpRequest, ":");
			if (findSearch == NULL) break;

			findSearchEnd = strstr(findSearch+1, "\r\n");
			if (findSearchEnd == NULL) break;

			tstring param(httpRequest, findSearch-httpRequest);
			tstring value(findSearch+2, findSearchEnd-findSearch-2);
			m_headers->setProperty(param, CGC_VALUEINFO(value));

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
						m_currentMultiPart->close();
						m_currentMultiPart->setParser(cgcNullParserBaseService);
						m_currentMultiPart.reset();
						return false;
					}
					m_currentMultiPart->setContentType(value);

					char tempSavePath[256];
					tstring::size_type findpath = theUpload.getTempPath().find("/");
					if (findpath == tstring::npos)
						findpath = theUpload.getTempPath().find("\\");
					if (findpath == tstring::npos)
					{
						sprintf(tempSavePath, "%s/%s", theApplication->getAppConfPath().c_str(), theUpload.getTempPath().c_str());
					}else
					{
						sprintf(tempSavePath, "%s", theUpload.getTempPath().c_str());
					}
					m_currentMultiPart->open(tempSavePath);

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
						m_currentMultiPart.reset();
						return false;
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
				if (m_contentSize > 0)
				{
					if (multipartyBoundary.empty())
					{
						const char * find = strstrl(httpRequest, "\r\n\r\n", strlen(httpRequest), 4);
						if (find != NULL)
							find += 4;
						size_t size = strlen(find);
						if (size == 0)
						{
							m_currentMultiPart = CGC_MULTIPART("");
							//return false;
						}else
						{
							int offset = 0;
							if (httpRequest[strlen(httpRequest)-1] == '\n')
								offset += 1;
							if (httpRequest[strlen(httpRequest)-2] == '\r')
								offset += 1;

							if (m_contentData)
								delete[] m_contentData;
							m_contentData = new char[m_contentSize+1];
							strncpy(m_contentData, httpRequest+strlen(httpRequest)-m_contentSize-offset, m_contentSize);
							m_contentData[m_contentSize] = '\0';
							m_queryString = tstring(m_contentData, m_contentSize);
						}
					}
				}
			}

			httpRequest = findSearchEnd + 2;
		}

		if (!multipartyBoundary.empty() && m_currentMultiPart.get() == NULL)
		{
			m_currentMultiPart = CGC_MULTIPART(multipartyBoundary);
			return false;
		}else if (m_currentMultiPart.get() != NULL && m_currentMultiPart->getBoundary().empty())
		{
			// 数据未收完整
			return false;
		}

		return true;
	}

	const char * strstrl(const char * sourceBuffer, const char * findBuffer, size_t sourceSize, size_t fineSize)
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

	bool sotpCompare(const char * pBuffer, const char * pCompare, int & leftIndex)
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

	bool isGreaterMaxSize(void) const
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
	unsigned char toHex(const unsigned char &x)
	{
		return x > 9 ? x -10 + 'A': x + '0';
	}

	unsigned char fromHex(const unsigned char &x)
	{
		return isdigit(x) ? x-'0' : x-'A'+10;
	}

	std::string URLEncode(const char *sIn)
	{
		std::string sOut;
		for( size_t ix = 0; ix < strlen(sIn); ix++ )
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
	};

	std::string URLDecode(const char *sIn)
	{
		std::string sOut;
		for( size_t ix = 0; ix < strlen(sIn); ix++ )
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
public:
	CParserHttp(void)
		: m_host("127.0.0.1"), m_account(""), m_secure(""), m_moduleName(""), m_functionName("doHttpFunc"), m_httpVersion("HTTP/1.1"), m_contentLength(0), m_method(HTTP_NONE)
		, m_requestURL(""), m_requestURI(""), m_queryString(""), m_fileName("")
		, m_keepAlive(true), m_keepAliveInterval(0), m_contentData(NULL), m_contentSize(0)
		, m_statusCode(STATUS_CODE_200), m_addDateHeader(false), m_sReqContentType(""), m_sResContentType("text/html"), m_sLocation("")
		, m_forwardFromURL("")
	{
		m_propertys = theApplication->createAttributes();
		m_headers = theApplication->createAttributes();
		assert (m_propertys.get() != NULL);
		assert (m_headers.get() != NULL);

		m_bodySize = 0;
		m_bodyBufferSize = INCREASE_BODY_SIZE;
		m_resultBuffer = new char[MAX_HTTPHEAD_SIZE+m_bodyBufferSize+1];
	}
	~CParserHttp(void)
	{
		//m_multiparts.clear();
		m_propertys->cleanAllPropertys();
		m_headers->cleanAllPropertys();

		if (m_contentData)
			delete[] m_contentData;
		delete[] m_resultBuffer;

		if (m_fileSystemService.get() != NULL)
		{
			for (size_t i=0; i<m_files.size(); i++)
			{
				m_fileSystemService->callService("delete", CGC_VALUEINFO(m_files[i]->getUploadFile()->getFilePath()));
			}
		}
		m_files.clear();
	}

	void setFileSystemService(cgcServiceInterface::pointer v) {m_fileSystemService = v;}
};*/

#include <CGCClass/cgcclassinclude.h>
using namespace cgc;

//class CParserHttp
//	: public CPpHttp
//{
//private:
//	virtual tstring serviceName(void) const {return _T("PARSERSOTP");}
//	virtual void stopService(void) {}
//};
#include <CGCBase/cgcServices.h>
cgcServiceInterface::pointer theFileSystemService;
tstring theXmlFile;

extern "C" bool CGC_API CGC_Module_Init(void)
{
	theXmlFile = theApplication->getAppConfPath();
	theXmlFile.append(_T("/upload.xml"));

	theFileSystemService = theServiceManager->getService("FileSystemService");
	if (theFileSystemService.get() == NULL)
	{
		CGC_LOG((cgc::LOG_ERROR, "FileSystemService Error.\n"));
	}else
	{
		//cgcValueInfo::pointer inProperty = CGC_VALUEINFO(xmlFile);
		//cgcValueInfo::pointer outProperty = CGC_VALUEINFO(false);
		//theFileSystemService->callService("exists", inProperty, outProperty);
		//if (outProperty->getBoolean())
		//{
		//	theUpload.load(theXmlFile);
		//}
	}
	return true;
}

extern "C" void CGC_API CGC_Module_Free(void)
{
	theFileSystemService.reset();
	//theUpload.clear();
}

extern "C" void CGC_API CGC_GetService(cgcServiceInterface::pointer & outService, const cgcValueInfo::pointer& parameter)
{
	CPpHttp * parserHttp = new CPpHttp();
	parserHttp->theUpload.load(theXmlFile);
	parserHttp->setFileSystemService(theFileSystemService);
	outService = cgcParserHttp::pointer(parserHttp);
}
