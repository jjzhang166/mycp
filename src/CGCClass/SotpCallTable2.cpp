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

#include "SotpCallTable2.h"
#include <boost/format.hpp>

namespace cgc
{
#ifdef _UNICODE
typedef boost::wformat tformat;
#else
typedef boost::format tformat;
#endif // _UNICODE

#ifdef WIN32
#include "windows.h"
std::string str_convert(const char * strSource, int sourceCodepage, int targetCodepage)
{
	int unicodeLen = MultiByteToWideChar(sourceCodepage, 0, strSource, -1, NULL, 0);
	if (unicodeLen <= 0) return "";

	wchar_t * pUnicode = new wchar_t[unicodeLen];
	memset(pUnicode,0,(unicodeLen)*sizeof(wchar_t));

	MultiByteToWideChar(sourceCodepage, 0, strSource, -1, (wchar_t*)pUnicode, unicodeLen);

	char * pTargetData = 0;
	int targetLen = WideCharToMultiByte(targetCodepage, 0, (wchar_t*)pUnicode, -1, (char*)pTargetData, 0, NULL, NULL);
	if (targetLen <= 0)
	{
		delete[] pUnicode;
		return "";
	}

	pTargetData = new char[targetLen];
	memset(pTargetData, 0, targetLen);

	WideCharToMultiByte(targetCodepage, 0, (wchar_t*)pUnicode, -1, (char *)pTargetData, targetLen, NULL, NULL);

	std::string result = pTargetData;
	//	tstring result(pTargetData, targetLen);
	delete[] pUnicode;
	delete[] pTargetData;
	return   result;
}
#endif

SotpCallTable2::SotpCallTable2(void)
: m_sEncoding(_T("GBK"))
, m_sSessionId(_T("")), m_sAppName(_T(""))
, m_sAccount(_T("")), m_sPasswd(_T(""))
, m_et(ModuleItem::ET_NONE)
, m_nCurrentCallId(0)
, m_nCurrentSeq(0)
{}

std::string SotpCallTable2::toAckString(unsigned short seq) const
{
	char lpszBuffer[60];
	sprintf(lpszBuffer,(_T("ACK SOTP/2.0\n")
		_T("Seq: %d\n")),seq);
	return lpszBuffer;
	//boost::format gFormatAckRequest(_T("ACK SOTP/2.0\n")
	//	_T("Seq: %d\n"));
	//return	std::string((gFormatAckRequest%seq).str());
}

std::string SotpCallTable2::toSesString(ProtocolType pt, const tstring & sValue, unsigned long cid, unsigned short seq, bool bNeedAck) const
{
	if (sValue.empty()) return _T("");
	std::string sProtocolType = _T("");
	std::string sNeedAck = _T("");
	switch (pt)
	{
	case PT_Open:
		sProtocolType = _T("OPEN");
		break;
	case PT_Close:
		sProtocolType = _T("CLOSE");
		break;
	case PT_Active:
		sProtocolType = _T("ACTIVE");
		break;
	}
/*
	unsigned int nBufferSize = 50+sValue.length();
	char * pResult = new char[nBufferSize];
	memset(pResult, 0, nBufferSize);
	int n = sprintf(pResult, _T("%s SOTP/2.0\n")
			_T("Cid: %d\n")
			_T("App: %s\n"), sProtocolType.c_str(), cid, sValue.c_str());
	pResult[n] = '\0';
	pOutSize = n;
	return pResult;
	*/
	if (bNeedAck)
	{
		sNeedAck = _T("NAck: 1\n");
		char lpszSeq[16];
		sprintf(lpszSeq,"Seq: %d\n",seq);
		sNeedAck.append(lpszSeq);
	}

	char lpszBuffer[250];
	if (pt == PT_Open)
	{
		sprintf(lpszBuffer,(_T("%s SOTP/2.0\n")
			_T("%s")
			_T("Cid: %lu\n")
			_T("App: %s\n")),sProtocolType.c_str(),sNeedAck.c_str(),cid,sValue.c_str());
		return lpszBuffer;
	}else
	{
		sprintf(lpszBuffer,(_T("%s SOTP/2.0\n")
			_T("%s")
			_T("Cid: %lu\n")
			_T("Sid: %s\n")),sProtocolType.c_str(),sNeedAck.c_str(),cid,sValue.c_str());
		return lpszBuffer;
	}
}

std::string SotpCallTable2::toOpenSesString(unsigned long cid, unsigned short seq, bool bNeedAck) const
{
	if (m_sAppName.empty()) return _T("");
	if (m_sAccount.empty())
	{
		return toSesString(PT_Open, m_sAppName, cid, seq, bNeedAck);
	}else
	{
		std::string sNeedAck = _T("");
		if (bNeedAck)
		{
			sNeedAck = _T("NAck: 1\n");
			char lpszSeq[16];
			sprintf(lpszSeq,"Seq: %d\n",seq);
			sNeedAck.append(lpszSeq);
		}

		/*
		unsigned int nBufferSize = 50+m_sAppName.length()+m_sAccount.length()+m_sPasswd.length();
		char * pResult = new char[nBufferSize];
		memset(pResult, 0, nBufferSize);
		int n = sprintf(pResult, _T("OPEN SOTP/2.0\n")
			_T("Cid: %d\n")
			_T("App: %s\n")
			_T("Ua: %s;pwd=%s;enc=%s\n"), cid, m_sAppName.c_str(), m_sAccount.c_str(), m_sPasswd.c_str(), ModuleItem::getEncryption(m_et).c_str());
		pResult[n] = '\0';
		pOutSize = n;
		return pResult;
		*/

		char lpszBuffer[1024];
		sprintf(lpszBuffer,(_T("OPEN SOTP/2.0\n")
			_T("%s")
			_T("Cid: %lu\n")
			_T("App: %s\n")
			_T("Ua: %s;pwd=%s;enc=%s\n")),sNeedAck.c_str(),cid,m_sAppName.c_str(),m_sAccount.c_str(),m_sPasswd.c_str(),ModuleItem::getEncryption(m_et).c_str());
		return lpszBuffer;
	}
}

std::string SotpCallTable2::toAppCallString(unsigned long cid, unsigned long nCallSign, const tstring & sCallName, unsigned short seq, bool bNeedAck)
{
	if (sCallName.empty()) return _T("");

	std::string sParameters = GetParametersString();
	m_parameters.clear();
	std::string sNeedAck = _T("");
	if (bNeedAck)
	{
		sNeedAck = _T("NAck: 1\n");
		char lpszSeq[16];
		sprintf(lpszSeq,"Seq: %d\n",seq);
		sNeedAck.append(lpszSeq);
	}

	char lpszBuffer[8*1024];
	if (m_sSessionId.empty())
	{
		// 2.0
		sprintf(lpszBuffer,(_T("CALL SOTP/2.0\n")
			_T("%s")
			_T("App: %s\n")
			_T("Ua: %s;pwd=%s;enc=\n")
			_T("Cid: %lu\n")
			_T("Sign: %lu\n")
			_T("Api: %s\n")),sNeedAck.c_str(),m_sAppName.c_str(),m_sAccount.c_str(),m_sPasswd.c_str(),cid,nCallSign,sCallName.c_str());
		tstring result(lpszBuffer);
		result.append(sParameters);
		return result;
	}else
	{
		sprintf(lpszBuffer,(_T("CALL SOTP/2.0\n")
			_T("%s")
			_T("Sid: %s\n")
			_T("Cid: %lu\n")
			_T("Sign: %lu\n")
			_T("Api: %s\n")),sNeedAck.c_str(),m_sSessionId.c_str(),cid,nCallSign,sCallName.c_str());
		tstring result(lpszBuffer);
		result.append(sParameters);
		return result;
	}
}

unsigned char * SotpCallTable2::toAttachString(cgcAttachment::pointer pAttach, unsigned int & pOutSize) const
{
	if (pAttach.get() == NULL || !pAttach->isHasAttach()) return NULL;
	unsigned int nBufferSize = 50+pAttach->getName().length()+pAttach->getAttachSize();
	unsigned char * pResult = new unsigned char[nBufferSize];
	memset(pResult, 0, nBufferSize);
	int n = sprintf((char*)pResult, "At: %s;at=%lld;ai=%lld;al=%d\n", pAttach->getName().c_str(), pAttach->getTotal(), pAttach->getIndex(), pAttach->getAttachSize());
	memcpy(pResult+n, pAttach->getAttachData(), pAttach->getAttachSize());
	pResult[n+pAttach->getAttachSize()] = '\n';
	pOutSize = n+pAttach->getAttachSize() + 1;
	return pResult;
}

std::string SotpCallTable2::toSessionResult(int prototype, unsigned long cid, long retCode, const tstring & sSessionId, unsigned short seq, bool bNeedAck) const
{
	std::string sNeedAck = _T("");
	if (bNeedAck)
	{
		sNeedAck = _T("NAck: 1\n");
		char lpszSeq[16];
		sprintf(lpszSeq,"Seq: %d\n",seq);
		sNeedAck.append(lpszSeq);
	}

	std::string sType = _T("");
	switch (prototype)
	{
	case PT_Open:
		sType = _T("OPEN");
		break;
	case PT_Close:
		sType = _T("CLOSE");
		break;
	case PT_Active:
		sType = _T("ACTIVE");
		break;
	case 10:
		sType = _T("OPEN");	// 用于临时打开SESSION
		break;
	default:
		sType = _T("UNKNOWN");
		break;
	}

	//tstring sValue;
//	if (m_sEncoding.compare(_T("UTF-8")) == 0)
//	{
//#ifdef WIN32
//		sValue = sSessionId;
//#else
//		cgcString::GB2312ToUTF_8(sValue, sSessionId.c_str(), sSessionId.length());
//#endif
//	}else
	//{
	//	sValue = sSessionId;
	//}

	char lpszBuffer[100];
	sprintf(lpszBuffer,(_T("%s SOTP/2.0 %ld\n")
		_T("%s")
		_T("Cid: %lu\n")
		_T("Sid: %s\n")),sType.c_str(),retCode,sNeedAck.c_str(),cid,sSessionId.c_str());
	return lpszBuffer;
}

std::string SotpCallTable2::toAppCallResult(unsigned long cid, unsigned long sign, long retCode, unsigned short seq, bool bNeedAck)
{
	std::string sNeedAck = _T("");
	if (bNeedAck)
	{
		sNeedAck = _T("NAck: 1\n");
		char lpszSeq[16];
		sprintf(lpszSeq,"Seq: %d\n",seq);
		sNeedAck.append(lpszSeq);
	}

	const std::string responseValues = GetParametersString();
	m_parameters.clear();
	char lpszBuffer[100];
	sprintf(lpszBuffer,(_T("CALL SOTP/2.0 %ld\n")
		_T("%s")
		_T("Cid: %lu\n")
		_T("Sign: %lu\n")),retCode,sNeedAck.c_str(),cid,sign);
	tstring result(lpszBuffer);
	result.append(responseValues);
	return result;
}
std::string SotpCallTable2::toP2PTry(void) const
{
	char lpszBuffer[32];
	sprintf(lpszBuffer,(_T("P2P SOTP/2.0\n")));
	return lpszBuffer;
}

void SotpCallTable2::setParameter(const cgcParameter::pointer & parameter, bool bSetForce)
{
	if (parameter.get() != NULL)
	{
		if (!bSetForce && parameter->getType() == cgcValueInfo::TYPE_STRING && parameter->empty())
			return;
		m_parameters.remove(parameter->getName());
		m_parameters.insert(parameter->getName(), parameter);
	}
}

void SotpCallTable2::addParameter(const cgcParameter::pointer & parameter, bool bAddForce)
{
	if (parameter.get() != NULL)
	{
		if (!bAddForce && parameter->getType() == cgcValueInfo::TYPE_STRING && parameter->empty())
			return;
		m_parameters.insert(parameter->getName(), parameter);
	}
}

void SotpCallTable2::addParameters(const std::vector<cgcParameter::pointer> & parameters, bool bAddForce)
{
	for (std::size_t i=0; i<parameters.size(); i++)
	{
		addParameter(parameters[i], bAddForce);
	}
}

unsigned long SotpCallTable2::getNextCallId(void)
{
	boost::mutex::scoped_lock lock(m_mutexCid);
	return (++m_nCurrentCallId)==0?1:m_nCurrentCallId;
}
unsigned short SotpCallTable2::getNextSeq(void)
{
	boost::mutex::scoped_lock lock(m_mutexReq);
	return ++m_nCurrentSeq;
}

std::string SotpCallTable2::GetParametersString(void) const
{
	//boost::format fRV(_T("Pv: %s;pt=%s;pl=%d\n%s\n"));
	char lpszBuffer[1024];
	std::string result = _T("");

	// ???
	boost::mutex::scoped_lock lock(const_cast<boost::mutex&>(m_parameters.mutex()));
	//long index=0;
	cgcParameterMap::const_iterator pConstIter;
	for (pConstIter=m_parameters.begin(); pConstIter!=m_parameters.end(); pConstIter++)
	{
		cgcParameter::pointer parameter = pConstIter->second;
		cgcValueInfo::ValueType nValueType = parameter->getType();
		tstring paramType(parameter->typeString());
		tstring paramName(parameter->getName());
		tstring paramValue(parameter->toString());
#ifdef WIN32
		if (nValueType == cgcValueInfo::TYPE_STRING && m_sEncoding == _T("UTF8"))
		{
			paramValue = str_convert(paramValue.c_str(),CP_ACP,CP_UTF8);
		}
#endif

		// 2.0
		size_t paramValueLength = paramValue.length();
#ifdef _UNICODE
		const wchar_t * pTemp = paramValue.c_str();
		size_t targetLen = 0;
		errno_t err = wcsrtombs_s(&targetLen, NULL, 0, &pTemp, paramValue.length(), NULL);
		if (err == 0 && targetLen > 0)
		{
			paramValueLength = targetLen-1;
		}
#endif // _UNICODE
		sprintf(lpszBuffer,"Pv: %s;pt=%s;pl=%d\n",paramName.c_str(),paramType.c_str(),paramValueLength);
		tstring sParam(lpszBuffer);
		sParam.append(paramValue);
		sParam.append("\n");
		//tstring sParam((fRV%paramName.c_str()%paramType.c_str()%paramValueLength%paramValue.c_str()).str());
#ifdef _DEBUG
		// ???		tstring sParamValueUtf8 = cgcString::Convert(parameter.getValue(), CP_ACP, CP_UTF8);
//		tstring sT(parameter.getValue());
//		tstring sParamValueGb2312 = cgcString::Convert(sT, CP_UTF8, CP_ACP);
#endif
		result.append(sParam);
		//index++;
	}
	return result;
}

} // cgc namespace

