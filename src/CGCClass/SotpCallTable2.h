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

// SotpCallTable2.h file here
#ifndef __SotpCallTable2_h__
#define __SotpCallTable2_h__

#include <vector>
#include "../CGCBase/cgcparameter.h"
#include "../CGCBase/cgcattachment.h"
#include "ModuleItem.h"

namespace cgc
{

class SotpCallTable2
{
public:
	enum ProtocolType
	{
		PT_Open		= 1
		, PT_Close	= 2
		, PT_Active	= 3
	};

public:
	SotpCallTable2(void);

	std::string toAckString(unsigned short seq) const;
	std::string toSesString(ProtocolType pt, const tstring & sValue, unsigned long cid, unsigned short seq, bool bNeedAck) const;
	std::string toOpenSesString(unsigned long cid, unsigned short seq, bool bNeedAck) const;
	std::string toAppCallString(unsigned long cid, unsigned long nCallSign, const tstring & sCallName, unsigned short seq, bool bNeedAck);
	unsigned char * toAttachString(cgcAttachment::pointer pAttach, unsigned int & pOutSize) const;
	// result
	std::string toSessionResult(int prototype, unsigned long cid, long retCode, const tstring & sSessionId, unsigned short seq, bool bNeedAck) const;
	std::string toAppCallResult(unsigned long cid, unsigned long sign, long retCode, unsigned short seq, bool bNeedAck);
	// p2p
	std::string toP2PTry(void) const;

public:
	// ENCODING
	void setEncoding(const tstring & newValue=_T("GBK")) {m_sEncoding = newValue;}
	const tstring & getEncoding(void) const {return m_sEncoding;}

	//
	// Inspects whether already opened the app session.
	bool isSessionOpened(void) const {return !m_sSessionId.empty();}
	const tstring & getSessionId(void) const {return m_sSessionId;}

	void setAppName(const tstring & newv) {m_sAppName = newv;}
	const tstring & getAppName(void) const {return m_sAppName;}

	void setAccount(const tstring & newv) {m_sAccount = newv;}
	const tstring & getAccount(void) const {return m_sAccount;}

	void setPasswd(const tstring & newv) {m_sPasswd = newv;}
	const tstring & getPasswd(void) const {return m_sPasswd;}

	void setEncryptionType(ModuleItem::EncryptionType newv){m_et = newv;}
	ModuleItem::EncryptionType getEncryption(void) {return m_et;}

	void setParameter(const cgcParameter::pointer & parameter, bool bSetForce=false);
	void addParameter(const cgcParameter::pointer & parameter, bool bAddForce=false);
	void addParameters(const std::vector<cgcParameter::pointer> & parameters, bool bAddForce=false);
	void delParameter(const tstring& paramName) {m_parameters.remove(paramName);}
	void clearParameter(void) {m_parameters.clear();}

	std::size_t getParameterSize(void) const {return m_parameters.size();}

	unsigned long getNextCallId(void);
	unsigned short getNextSeq(void);

protected:
	std::string GetParametersString(void) const;

protected:
	boost::mutex m_mutexCid;
	boost::mutex m_mutexReq;

	cgcParameterMap m_parameters;
	tstring m_sEncoding;
	tstring m_sSessionId;
	unsigned int m_nDataIndex;
	tstring m_sAppName;
	tstring m_sAccount;
	tstring m_sPasswd;
	ModuleItem::EncryptionType m_et;

private:
	unsigned long m_nCurrentCallId;
	unsigned short m_nCurrentSeq;
};

} // cgc namespace

#endif // __SotpCallTable2_h__
