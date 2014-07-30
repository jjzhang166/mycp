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

#include "PpSotp2.h"
//#include <boost/format.hpp>

namespace cgc
{
/*
#ifdef _UNICODE
typedef boost::wformat tformat;
#else
typedef boost::format tformat;
#endif // _UNICODE
*/

CPPSotp2::CPPSotp2(void)
{
	m_attach = cgcAttachment::create();
}

//////////////////////////////////////////////
// Request
bool CPPSotp2::doParse(const unsigned char * requestData, size_t size,const char* sEncoding)
{
	return m_cgcInvoke.parseBuffer(requestData,sEncoding);
}

/*
tstring CPPSotp2::getRecvParameterValue(const tstring & sParamName, const tstring & sDefault) const
{
	cgcParameter::pointer param = getRecvParameter(sParamName);
	if (param.get() == NULL) return sDefault;
	if (param->getType() != cgcParameter::TYPE_STRING)
	{
		cgcValueInfo::pointer temp = param->copy();
		temp->totype(cgcParameter::TYPE_STRING);
		return temp->getStr();
	}
	return param->getStr();
}

int CPPSotp2::getRecvParameterValue(const tstring & sParamName, int nDefault) const
{
	cgcParameter::pointer param = getRecvParameter(sParamName);
	if (param.get() == NULL) return nDefault;
	if (param->getType() != cgcParameter::TYPE_INT)
	{
		cgcValueInfo::pointer temp = param->copy();
		temp->totype(cgcParameter::TYPE_INT);
		return temp->getInt();
	}
	return param->getInt();
}

bool CPPSotp2::getRecvParameterValue(const tstring & sParamName, bool bDefault) const
{
	cgcParameter::pointer param = getRecvParameter(sParamName);
	if (param.get() == NULL) return bDefault;
	if (param->getType() != cgcParameter::TYPE_BOOLEAN)
	{
		cgcValueInfo::pointer temp = param->copy();
		temp->totype(cgcParameter::TYPE_BOOLEAN);
		return temp->getBoolean();
	}
	return param->getBoolean();
}

time_t CPPSotp2::getRecvParameterValue(const tstring & sParamName, time_t tDefault) const
{
	cgcParameter::pointer param = getRecvParameter(sParamName);
	if (param.get() == NULL) return tDefault;
	if (param->getType() != cgcParameter::TYPE_TIME)
	{
		cgcValueInfo::pointer temp = param->copy();
		temp->totype(cgcParameter::TYPE_TIME);
		return temp->getTime();
	}
	return param->getTime();
}

double CPPSotp2::getRecvParameterValue(const tstring & sParamName, double fDefault) const
{
	cgcParameter::pointer param = getRecvParameter(sParamName);
	if (param.get() == NULL) return fDefault;
	if (param->getType() != cgcParameter::TYPE_FLOAT)
	{
		cgcValueInfo::pointer temp = param->copy();
		temp->totype(cgcParameter::TYPE_FLOAT);
		return temp->getFloat();
	}
	return param->getFloat();
}
*/

////////////////////////////////////////////////////////
// Response
std::string CPPSotp2::getSessionResult(long retCode, const tstring & sSessionId, unsigned short seq, bool bNeedAck) const
{
	return toSessionResult(getProtoType(), getCallid(), retCode, sSessionId, seq, bNeedAck);
//	std::string sNeedAck = _T("");
//	if (bNeedAck)
//	{
//		sNeedAck = _T("NAck: 1\n");
//	}
//
//	std::string sType = _T("");
//	switch (getProtoType())
//	{
//	case 1:
//		sType = _T("OPEN");
//		break;
//	case 2:
//		sType = _T("CLOSE");
//		break;
//	case 3:
//		sType = _T("ACTIVE");
//		break;
//	case 10:
//		sType = _T("OPEN");	// 用于临时打开SESSION
//		break;
//	default:
//		sType = _T("UNKNOWN");
//		break;
//	}
//
//	tstring sValue;
////	if (m_sEncoding.compare(_T("UTF-8")) == 0)
////	{
////#ifdef WIN32
////		sValue = sSessionId;
////#else
////		cgcString::GB2312ToUTF_8(sValue, sSessionId.c_str(), sSessionId.length());
////#endif
////	}else
//	{
//		sValue = sSessionId;
//	}
//
//	boost::format gFormatSesResponse(_T("%s SOTP/2.0 %d\n")
//		_T("Seq: %d\n%s")
//		_T("Cid: %d\n")
//		_T("Sid: %s\n"));
//
//	std::string responseData((gFormatSesResponse%sType.c_str()%retCode%seq%sNeedAck.c_str()%getCallid()%sValue.c_str()).str());
//	return responseData;
}

std::string CPPSotp2::getAppCallResult(long retCode, unsigned short seq, bool bNeedAck)
{
	return toAppCallResult(getCallid(), getSign(), retCode, seq, bNeedAck);
//	std::string sNeedAck = _T("");
//	if (bNeedAck)
//	{
//		sNeedAck = _T("NAck: 1\n");
//	}
//
//	std::string responseValues = GetParametersString();
//	m_parameters.clear();
//	boost::format gFormatResponse(_T("CALL SOTP/2.0 %d\n")
//		_T("Seq: %d\n%s")
//		_T("Cid: %d\n")
//		_T("Sign: %d\n")
//		_T("%s"));		// parameters
////	if (m_attach.isHasAttach())
////	{
////	}
//	return std::string((gFormatResponse%retCode%seq%sNeedAck.c_str()%getCallid()%getSign()%responseValues.c_str()).str());
}
std::string CPPSotp2::getAckResult(unsigned short seq)
{
	return toAckString(seq);
	//boost::format gFormatResponse(_T("ACK SOTP/2.0\n")
	//	_T("Seq: %d\n"));
	//return std::string((gFormatResponse%seq).str());
}
void CPPSotp2::setResAttachName(const tstring & name)
{
	m_attach->setName(name);
}

void CPPSotp2::setResAttachInfo(cgc::bigint total, cgc::bigint index)
{
	m_attach->setTotal(total);
	m_attach->setIndex(index);
}

void CPPSotp2::setResAttachData(const unsigned char * attachData, unsigned int attachSize)
{
	m_attach->setAttach(attachData, attachSize);
}

} // cgc namespace
