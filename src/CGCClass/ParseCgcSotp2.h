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

// ParseCgcSotp2.h file here
#ifndef __ParseCgcSotp2_h__
#define __ParseCgcSotp2_h__

//#include <boost/lexical_cast.hpp>
#include "../CGCBase/cgcparameter.h"
#include "../CGCBase/cgcattachment.h"
#include "../CGCBase/cgcParserSotp.h"
#include "ModuleItem.h"
#include "MethodItem.h"
//#include "info/ClusterSvr.h"

#include "dlldefine.h"
using namespace cgc;

class CGCCLASS_CLASS ParseCgcSotp2
{
public:
	ParseCgcSotp2(void);
	virtual ~ParseCgcSotp2(void);

public:
	bool isSessionProto(void) const {return m_nCgcProto==1;}
	bool isAppProto(void) const {return m_nCgcProto==2;}
	void setAppProto(void) {m_nCgcProto=2;}
	bool isAckProto(void) const {return m_nCgcProto==3;}
	bool isP2PProto(void) const {return m_nCgcProto==4;}
	//bool isClusterProto(void) const {return m_nCgcProto==3;}
	int getProtoType(void) const {return m_nProtoType;}

	void setAppName(const tstring & newv) {m_sApp = newv;}
	const tstring & getAppName(void) const {return m_sApp;}

	void setApiName(const tstring & newv) {m_sApi = newv;}
	const tstring & getApiName(void) const {return m_sApi;}

	//void setProtoValue(const tstring & newv) {m_sProtoValue = newv;}
	//const tstring & getProtoValue(void) const {return m_sProtoValue;}

	bool isOpenType(void) const {return m_nProtoType==1;}
	bool isCloseType(void) const {return m_nProtoType==2;}
	bool isActiveType(void) const {return m_nProtoType==3;}
	void setCallType(void) {m_nProtoType = 10;}
	bool isCallType(void) const {return m_nProtoType==10;}
	bool isQueryType(void) const {return m_nProtoType==20;}
	bool isVerifyType(void) const {return m_nProtoType==21;}

	bool isResulted(void) const {return m_bResulted;}
//	const tstring & getResultString(void) const {return m_sResultValue;}
	long getResultValue(void) const {return m_nResultValue;}

	void setAccount(const tstring & newv) {m_sAccount = newv;}
	const tstring & getAccount(void) const {return m_sAccount;}

	void setPasswd(const tstring & newv) {m_sPasswd = newv;}
	const tstring & getPasswd(void) const {return m_sPasswd;}

	bool hasSeq(void) const {return m_bHasSeq;}
	unsigned short getSeq(void) const {return m_seq;}
	bool isNeedAck(void) const {return m_bNeedAck;}
	void setSid(const tstring & newValue) {m_sSid = newValue;}
	const tstring & getSid(void) const {return m_sSid;}
	void setSslPublicKey(const tstring & newValue) {m_sSslPublicKey = newValue;}
	bool isSslRequest(void) const {return m_sSslPublicKey.empty()?false:true;}
	const tstring & getSslPublicKey(void) const {return m_sSslPublicKey;}
	//void setSslPrivateKey(const tstring & newValue) {m_sSslPrivateKey = newValue;}
	//const tstring & getSslPrivateKey(void) const {return m_sSslPrivateKey;}
	//void setSslPrivatePwd(const tstring & newValue) {m_sSslPrivatePwd = newValue;}
	//const tstring & getSslPrivatePwd(void) const {return m_sSslPrivatePwd;}
	//void setSslPassword(const tstring & newValue) {m_sSslPassword = newValue;}
	const tstring & getSslPassword(void) const {return m_sSslPassword;}
	void setCallid(unsigned long newValue) {m_nCallId = newValue;}
	unsigned long getCallid(void) const {return m_nCallId;}
	void setSign(unsigned long newValue) {m_nSign = newValue;}
	unsigned long getSign(void) const {return m_nSign;}

	//
	// parameter
	size_t getParameterCount(void) const {return this->m_parameterMap.size();}
	const cgcParameterMap & getParameters(void) const {return this->m_parameterMap;}
	cgcParameter::pointer getParameter(const tstring & paramName) const {return m_parameterMap.getParameter(paramName);}
	bool getParameter(const tstring & paramName, std::vector<cgcParameter::pointer>& outParams) const {return m_parameterMap.find(paramName, outParams);}
	tstring getParameterValue(const tstring & paramName, const char* defaultValue) const {return m_parameterMap.getParameterValue(paramName, defaultValue);}
	int getParameterValue(const tstring & paramName, int defaultValue) const {return m_parameterMap.getParameterValue(paramName, defaultValue);}
	bigint getParameterValue(const tstring & paramName, bigint defaultValue) const {return m_parameterMap.getParameterValue(paramName, defaultValue);}
	bool getParameterValue(const tstring & paramName, bool defaultValue) const {return m_parameterMap.getParameterValue(paramName, defaultValue);}
	double getParameterValue(const tstring & paramName, double defaultValue) const {return m_parameterMap.getParameterValue(paramName, defaultValue);}

	cgcAttachment::pointer getAttachInfo(void) const {return m_attach;}

	//
	// cluster
//	size_t getClusterSvrCount(void) const {return this->m_custerSvrList.size();}
//	const ClusterSvrList & getClusters(void) const {return m_custerSvrList;}
//	int getClusters(ClusterSvrList & listResult);

	void FreeHandle(void);
	void addParameter(const cgcParameter::pointer& parameter);

public:
	void setParseCallback(cgc::cgcParserCallback* pCallback) {m_pCallback = pCallback;}
	bool parseBuffer(const unsigned char * pBuffer,const char* sEncoding="");

protected:
	// 把SOTP协议，改到2.0版本
	const char * parseOneLine(const char * pLineBuffer);
	bool sotpCompare(const char * pBuffer, const char * pCompare, int & leftIndex);

private:
	tstring m_sEncoding;
	cgcParameterMap m_parameterMap;
	cgcAttachment::pointer m_attach;
	cgcParserCallback* m_pCallback;
	//ClusterSvrList m_custerSvrList;

	int m_nCgcProto;		// 1: session, 2: app, 3: cluster 4: p2p
	int m_nProtoType;		// 1:open, 2:close, 3:active, 10:call, 20:query, 21:verify
	//tstring m_sProtoValue;
	bool m_bHasSeq;
	unsigned short m_seq;
	bool m_bNeedAck;		// default false
	tstring m_sSid;				// session id
	tstring m_sApp;				// app name
	tstring m_sApi;				// api name
	tstring m_sSslPublicKey;
	//tstring m_sSslPrivateKey;
	//tstring m_sSslPrivatePwd;
	tstring m_sSslPassword;			// *
	unsigned char* m_pSslDecryptData;
	unsigned long m_nCallId;			// call id
	unsigned long m_nSign;				// sign
	bool m_bResulted;
//	tstring m_sResultValue;
	long m_nResultValue;

	tstring m_sAccount;		// for open session, query cluster
	tstring m_sPasswd;
	ModuleItem::EncryptionType m_et;
};


#endif // __ParseCgcSotp2_h__
