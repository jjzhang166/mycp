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

// SotpRtpSession.h file here
#ifndef __SotpRtpSession_h__
#define __SotpRtpSession_h__

#include <CGCBase/cgcParserSotp.h>
#include <ThirdParty/stl/lockmap.h>
#include "SotpRtpRoom.h"
//#include "SotpRtpSink.h"

namespace cgc
{


class CSotpRtpSession
{
public:
	void SetCallback(HSotpRtpFrameCallback pCallback) {m_pCallback = pCallback;}
	void SetCbUserData(void* nUserData) {m_nCbUserData = nUserData;}
	bool doRtpCommand(const tagSotpRtpCommand& pRtpCommand, const cgcRemote::pointer& pcgcRemote,bool bSendRtpCommand);
	void UnRegisterAllRoomSink(cgc::bigint nSrcId);
	bool doRtpData(const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttackment, const cgcRemote::pointer& pcgcRemote);

	void CheckRegisterSourceLive(short nExpireSecond);	// for server
	void CheckRegisterSinkLive(short nExpireSecond, const cgcRemote::pointer& pcgcRemote);	// for client

	void GetRoomIdList(std::vector<cgc::bigint>& pOutRoomIdList) const;
	bool IsRegisterSource(cgc::bigint nRoomId, cgc::bigint nSrcId) const;
	bool IsRegisterSink(cgc::bigint nRoomId, cgc::bigint nSrcId, cgc::bigint nDestId) const;

	CSotpRtpRoom::pointer GetRtpRoom(cgc::bigint nRoomId,bool bCreateNew);
	CSotpRtpRoom::pointer GetRtpRoom(cgc::bigint nRoomId) const;

	CSotpRtpSession(bool bServerMode);
	virtual ~CSotpRtpSession(void);
private:
	bool m_bServerMode;
	HSotpRtpFrameCallback m_pCallback;
	void* m_nCbUserData;
	CLockMap<cgc::bigint,CSotpRtpRoom::pointer> m_pRoomList;
};

} // cgc namespace

#endif // __SotpRtpSession_h__