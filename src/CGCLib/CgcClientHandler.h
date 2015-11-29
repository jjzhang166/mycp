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

// CgcClientHandler.h file here
#ifndef __CgcClientHandler_h__
#define __CgcClientHandler_h__

//
// include
#include "IncludeBase.h"
#include "CgcData.h"

namespace cgc
{
	const cgcAttachment::pointer constNullAttchment;

	///////////////////////////////////////
	// Response handler
	// CgcClientHandler class
	class CgcClientHandler
	{
	public:
		virtual void OnCgcResponse(const cgcParserSotp & response)=0;
		virtual void OnRtpFrame(cgc::bigint nSrcId, const CSotpRtpFrame::pointer& pRtpFrame, cgc::uint16 nLostCount, cgc::uint32 nUserData){}
		virtual void OnCgcResponse(const unsigned char * data, size_t size){}	// disable CGCParser or parser Error
		//virtual void OnCgcResponse(const CCgcData::pointer& recvData){}	// disable CGCParser or parser Error

		virtual void OnActiveTimeout(void) {}
		//
		// bool canResendAgain:
		//  true : resend this callid data
		//  false : do not resend this callid 
		virtual void OnCidTimeout(unsigned long callid, unsigned long sign, bool canResendAgain) {}
	};

	// 
	enum SOTP_CLIENT_CONFIG_TYPE
	{
		SOTP_CLIENT_CONFIG_MAX_RECEIVE_BUFFER_SIZE	= 1
		, SOTP_CLIENT_CONFIG_USES_SSL						// 0/1 (set only)
		, SOTP_CLIENT_CONFIG_PUBLIC_KEY						// const char* (set only)
		, SOTP_CLIENT_CONFIG_PRIVATE_KEY					// const char* (set only)
		, SOTP_CLIENT_CONFIG_PUBLIC_FILE					// const char* (set only)
		, SOTP_CLIENT_CONFIG_PRIVATE_FILE					// const char* (set only)
		, SOTP_CLIENT_CONFIG_PRIVATE_PWD					// const char* (set only)
		, SOTP_CLIENT_CONFIG_HAS_SSL_PASSWORD				// 0/1 (get only)
		, SOTP_CLIENT_CONFIG_SOTP_VERSION			= 10	// 20,21 default 20
		, SOTP_CLIENT_CONFIG_CURRENT_INDEX					// for sid and ssl pwd,...
		, SOTP_CLIENT_CONFIG_RTP_CB_USERDATA				// for OnRtpFrame
		, SOTP_CLIENT_CONFIG_TRAN_SPEED_LIMIT				// KB/S default 64KB/S
		, SOTP_CLIENT_CONFIG_IO_SEND_BUFFER_SIZE	= 20	// default 64 KB
		, SOTP_CLIENT_CONFIG_IO_RECEIVE_BUFFER_SIZE			// default 64 KB
	};

	class DoSotpClientHandler
	{
	public:
		typedef boost::shared_ptr<DoSotpClientHandler> pointer;

		// response handler
		virtual void doSetResponseHandler(CgcClientHandler * newv) = 0;
		virtual const CgcClientHandler * doGetResponseHandler(void) const  = 0;
		virtual void doSetDisableSotpParser(bool newv) = 0;		// default false

		virtual bool doSetConfig(int nConfig, unsigned int nInValue) = 0;
		virtual void doGetConfig(int nConfig, unsigned int* nOutValue) const = 0;
		virtual void doFreeConfig(int nConfig, unsigned int nInValue) const = 0;

		// session 
		virtual bool doSendOpenSession(short nMaxWaitSecons=3,unsigned long * pOutCallId = 0) = 0;
		virtual void doSendCloseSession(unsigned long * pOutCallId = 0) = 0;
		virtual bool doIsSessionOpened(void) const = 0;
		virtual const tstring & doGetSessionId(void) const = 0;

		// app call
		virtual void doBeginCallLock(void) = 0;
		virtual bool doSendAppCall(unsigned long nCallSign, const tstring & sCallName, bool bNeedAck = true,
			const cgcAttachment::pointer& pAttach = constNullAttchment, unsigned long * pOutCallId = 0) = 0;
		virtual bool doSendCallResult(long nResult,unsigned long nCallId,unsigned long nCallSign,bool bNeedAck = true,const cgcAttachment::pointer& pAttach = constNullAttchment) = 0;
		virtual void doSendP2PTry(unsigned short nTryCount=3) = 0;

		// sotp rtp
		virtual void doSetRtpSourceId(cgc::bigint nSrcId) = 0;
		virtual cgc::bigint doGetRtpSourceId(void) const = 0;
		virtual bool doRegisterSource(cgc::bigint nRoomId, cgc::bigint nParam) = 0;
		virtual void doUnRegisterSource(cgc::bigint nRoomId) = 0;
		virtual bool doIsRegisterSource(cgc::bigint nRoomId) const = 0;
		virtual void doUnRegisterAllSource(void) = 0;
		virtual bool doRegisterSink(cgc::bigint nRoomId, cgc::bigint nDestId) = 0;
		virtual void doUnRegisterSink(cgc::bigint nRoomId, cgc::bigint nDestId) = 0;
		virtual void doUnRegisterAllSink(cgc::bigint nRoomId) = 0;
		virtual void doUnRegisterAllSink(void) = 0;
		virtual bool doIsRegisterSink(cgc::bigint nRoomId, cgc::bigint nDestId) const = 0;
		virtual bool doSendRtpData(cgc::bigint nRoomId,const unsigned char* pData,cgc::uint32 nSize,cgc::uint32 nTimestamp=0,cgc::uint8 nDataType=SOTP_RTP_NAK_DATA_AUDIO,cgc::uint8 nNAKType=SOTP_RTP_NAK_REQUEST_1) = 0;

		// thread
		virtual void doSetCIDTResends(unsigned short timeoutResends=2, unsigned short timeoutSeconds=4) = 0;
		//virtual void doStartRecvThreads(unsigned short nRecvThreads = 2) = 0; // ** not use
		virtual void doStartActiveThread(unsigned short nActiveWaitSeconds = 30,unsigned short nSendP2PTrySeconds=0) = 0;

		// parameter
		virtual void doAddParameter(const cgcParameter::pointer& parameter, bool bAddForce = true) = 0;
		virtual void doAddParameters(const std::vector<cgcParameter::pointer>& parameters, bool bAddForce = true) = 0;
		virtual size_t doGetParameterSize(void) const = 0;

		// info
		virtual void doSetEncoding(const tstring & newv=_T("GBK")) = 0;
		virtual const tstring & doGetEncoding(void) const = 0;
		virtual void doSetAppName(const tstring & newv) = 0;
		virtual const tstring & doGetAppName(void) const = 0;
		virtual void doSetAccount(const tstring & account, const tstring & passwd) = 0;
		virtual void doGetAccount(tstring & account, tstring & passwd) const = 0;
		virtual const tstring & doGetClientType(void) const = 0;

		// other
		virtual time_t doGetLastSendRecvTime(void) const = 0;
		virtual bool doSetRemoteAddr(const tstring & newv) = 0;
		virtual tstring doGetRemoteAddr(void) const = 0;
		virtual const tstring& doGetLocalIp(void) const = 0;
		virtual unsigned short doGetLocalPort(void) const = 0;
		virtual void doSetMediaType(unsigned short newv) = 0;	// ?for RTP
		virtual size_t doSendData(const unsigned char * data, size_t size) = 0;
		virtual size_t doSendData(const unsigned char * data, size_t size, unsigned int timestamp) = 0;	// ?for RTP
	};

	//

} // namespace cgc
#endif // __CgcClientHandler_h__
