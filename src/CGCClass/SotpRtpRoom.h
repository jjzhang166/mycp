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

// SotpRtpRoom.h file here
#ifndef __SotpRtpRoom_h__
#define __SotpRtpRoom_h__

#include "../ThirdParty/stl/lockmap.h"
#include "SotpRtpSource.h"

namespace cgc
{
//typedef void (FAR *SinkExpireCallback) (void* pUserData, void* pRtpRoom,void* pRtpSourc);
class CSotpRtpCallback
{
public:
	virtual bool onRegisterSource(cgc::bigint nRoomId, cgc::bigint nSourceId, cgc::bigint nParam, void* pUserData) = 0;
	virtual bool onRegisterSink(cgc::bigint nRoomId, cgc::bigint nSourceId, cgc::bigint nDestId, void* pUserData) = 0;
};

class CSotpRtpRoom
{
public:
	typedef boost::shared_ptr<CSotpRtpRoom> pointer;
	static CSotpRtpRoom::pointer create(bool bServerMode, cgc::bigint nRoomId)
	{
		return CSotpRtpRoom::pointer(new CSotpRtpRoom(bServerMode, nRoomId));
	}
	bool IsServerMode(void) const {return m_bServerMode;}
	cgc::bigint GetRoomId(void) const {return m_nRoomId;}
	bool IsEmpty(void) const {return m_pSourceList.empty();}

	CSotpRtpSource::pointer RegisterSource(cgc::bigint nSrcId, cgc::bigint nParam, const cgcRemote::pointer& pcgcRemote, CSotpRtpCallback* pCallback, void* pUserData);
	bool UnRegisterSource1(cgc::bigint nSrcId, cgc::bigint* pOutParam);	// for client
	bool UnRegisterSource2(cgc::bigint nSrcId, cgc::bigint nParam);		// for server
	bool RegisterSink(cgc::bigint nSrcId,cgc::bigint nDestId,const cgcRemote::pointer& pcgcRemote, CSotpRtpCallback* pCallback, void* pUserData);
	bool UnRegisterSink(cgc::bigint nSrcId,cgc::bigint nDestId,const cgcRemote::pointer& pcgcRemote);
	bool UnRegisterAllSink(cgc::bigint nSrcId, const cgcRemote::pointer& pcgcRemote);
	bool UnRegisterAllSink(const CSotpRtpSource::pointer& pRtpSrcSource, const cgcRemote::pointer& pcgcRemote);

	CSotpRtpSource::pointer GetRtpSource(cgc::bigint nSrcId) const;
	bool IsRegisterSource(cgc::bigint nSrcId) const;
	void BroadcastRtpData(const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttackment) const;
	void CheckRegisterSourceLive(time_t tNow,short nExpireSecond);
	void CheckRegisterSinkLive(time_t tNow,short nExpireSecond,cgc::bigint nSrcId, const cgcRemote::pointer& pcgcRemote);	// for client

	void ClearAll(void);

	CSotpRtpRoom(bool bServerMode, cgc::bigint nRoomId);
	virtual ~CSotpRtpRoom(void);
private:
	bool m_bServerMode;
	cgc::bigint m_nRoomId;
	CLockMap<cgc::bigint,CSotpRtpSource::pointer> m_pSourceList;
};

} // cgc namespace

#endif // __SotpRtpRoom_h__
