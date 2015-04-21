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

// cgcrtpobject.h file here
#ifndef __cgcrtpobject_h__
#define __cgcrtpobject_h__

#include "cgcdef.h"
#include "cgcobject.h"

namespace cgc {

struct tagSotpRtpDataRequest
{
	cgc::uint16			m_nSeq;
	cgc::uint16			m_nCount;
};
struct tagSotpRtpCommand
{
	cgc::uint8			m_nCommand;
	cgc::bigint			m_nRoomId;
	cgc::bigint			m_nSrcId;
	union
	{
		cgc::bigint				m_nDestId;
		tagSotpRtpDataRequest	m_nDataRequest;	// for SOTP_RTP_COMMAND_DATA_REQUEST
	}u;
};
#define SOTP_RTP_COMMAND_SIZE sizeof(tagSotpRtpCommand)
//#define SOTP_RTP_COMMAND_SIZE 25	// 1+3*8=25
struct tagSotpRtpDataHead
{
	cgc::bigint		m_nRoomId;
	cgc::bigint		m_nSrcId;
	cgc::uint16		m_nSeq;
	cgc::uint8		m_nNAKType;		// see SOTP_RTP_NAK_TYPE
	cgc::uint8		m_nDataType;	// see SOTP_RTP_DATA_TYPE
	cgc::uint32		m_nTimestamp;
	cgc::uint16		m_nTotleLength;
	cgc::uint16		m_nUnitLength;
	cgc::uint8		m_nIndex;
};
#define SOTP_RTP_DATA_HEAD_SIZE sizeof(tagSotpRtpDataHead)
//#define SOTP_RTP_DATA_HEAD_SIZE 29
//#define SOTP_RTP_DATA_MAX_UNIT_SIZE 1200*6

#define SOTP_RTP_MAX_PACKETS_PER_FRAME	64
#define SOTP_RTP_MAX_PAYLOAD_LENGTH		1100

class CSotpRtpFrame
{
public:
	typedef boost::shared_ptr<CSotpRtpFrame> pointer;
	static CSotpRtpFrame::pointer create(void)
	{
		return CSotpRtpFrame::pointer(new CSotpRtpFrame());
	}
	static CSotpRtpFrame::pointer create(const tagSotpRtpDataHead& pRtpHead)
	{
		return CSotpRtpFrame::pointer(new CSotpRtpFrame(pRtpHead));
	}

	cgc::uint16			m_nFirstSeq;
	cgc::uint8			m_nPacketNumber;
	cgc::uint8			m_nFilled[SOTP_RTP_MAX_PACKETS_PER_FRAME];   
	cgc::uint32			m_nExpireTime;
	tagSotpRtpDataHead	m_pRtpHead;
	char*				m_pPayload;
	CSotpRtpFrame(void)
		: m_nFirstSeq(0)
		, m_nPacketNumber(0)
		, m_nExpireTime(0)
		, m_pPayload(NULL)
	{
		memset(&m_nFilled,0,sizeof(m_nFilled));
		memset(&m_pRtpHead,0,SOTP_RTP_DATA_HEAD_SIZE);
	}
	CSotpRtpFrame(const tagSotpRtpDataHead&	pRtpHead)
		: m_nFirstSeq(0)
		, m_nPacketNumber(0)
		, m_nExpireTime(0)
		, m_pPayload(NULL)
	{
		memset(&m_nFilled,0,sizeof(m_nFilled));
		memcpy(&m_pRtpHead,&pRtpHead,SOTP_RTP_DATA_HEAD_SIZE);
	}
	virtual ~CSotpRtpFrame(void)
	{
		if (m_pPayload!=NULL)
			delete[] m_pPayload;
	}
};

//typedef void (* HSotpRtpFrameCallback)(cgc::bigint nSrcId, const CSotpRtpFrame::pointer& pRtpFrame, cgc::uint16 nLostCount, cgc::uint32 nUserData);

} // namespace cgc


#endif // __cgcrtpobject_h__
