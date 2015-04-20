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

// SotpRtpSource.h file here
#ifndef __SotpRtpSource_h__
#define __SotpRtpSource_h__

#include <boost/thread.hpp>
#include "../CGCBase/cgcrtpobject.h"
#include "../CGCBase/cgcattachment.h"
#include "../CGCBase/cgcCommunications.h"

namespace cgc
{
#define RELIABLE_QUEUE_SIZE 256
#define MAX_RESEND_COUNT 5			// old=10 大概一秒左右数据量 70
typedef void (* HSotpRtpFrameCallback)(cgc::bigint nSrcId, const CSotpRtpFrame::pointer& pRtpFrame, cgc::uint16 nLostCount, void* nUserData);

struct compare_seq
{
	bool operator()(unsigned short seqa, unsigned short seqb)
	{
		// 升序排列:a小于b,返回true
		const int intever = seqa - seqb;
		if (intever > 0 && intever < 0x7F00)
			return false;	// a大于b,返回false,排前面
		else if (intever < 0 && (seqb - seqa) > 0x7F00)
			return false;

		return true;
	}
};
class CLostSeqInfo
{
public:
	void clear(void) {m_lostSeq.clear();}
	bool recv(unsigned short seq, unsigned short lastseq, unsigned short & pOutSendSeq, int & pOutSendCount, bool bSend)
	{
		std::list<unsigned short>::iterator pIter;
		for (pIter=m_lostSeq.begin(); pIter!=m_lostSeq.end(); ++pIter)
		{
			unsigned short pLostSeqId = *pIter;
			if (pLostSeqId == seq)
			{
				// 已经收到数据，删除掉
				m_lostSeq.erase(pIter);
				break;
			}
		}
		if (!bSend)
			return false;
		return send(lastseq, pOutSendSeq, pOutSendCount);
	}

	void lost(unsigned short seq, int count)
	{
		for (unsigned short i=0; i<count; i++)
		{
			lost(seq+i);
		}
		m_lostSeq.sort(compare_seq());
		while (m_lostSeq.size() >= MAX_RESEND_COUNT*4)
			m_lostSeq.pop_front();
	}

	bool send(unsigned short lastSeq, unsigned short & pOutSendSeq, int & pOutSendCount)
	{
		std::list<unsigned short>::iterator pIter;
		pIter = m_lostSeq.begin();
		if (pIter == m_lostSeq.end())
			return false;
		unsigned short pLostSeqId = *pIter;
		unsigned short sendSeq = pLostSeqId;
		if (lastSeq > sendSeq)
		{
			if (lastSeq - sendSeq < m_offset1)
				return false;						// 相差x个数据,暂时不处理,用于节省流量
			else if (lastSeq - sendSeq > m_offset2)
			{
				m_lostSeq.pop_front();				// 过期数据不处理
				return send(lastSeq, pOutSendSeq, pOutSendCount);
			}
		}

		int sendCount = 0;
		do
		{
			sendCount++;
			m_lostSeq.pop_front();
			if (m_lostSeq.empty())		// 已经清空
				break;

			pIter = m_lostSeq.begin();
			pLostSeqId = *pIter;
			if (pLostSeqId != (sendSeq+sendCount))
				break;
		}while (sendCount < MAX_RESEND_COUNT);

		pOutSendSeq = sendSeq;
		pOutSendCount = sendCount;
		return true;
	}

	size_t size(void) const
	{
		return m_lostSeq.size();
	}
	CLostSeqInfo(int offset1, int offset2)
		: m_offset1(offset1), m_offset2(offset2)
	{
	}
protected:
	void lost(unsigned short seq)
	{
		std::list<unsigned short>::iterator pIter;
		for (pIter=m_lostSeq.begin(); pIter!=m_lostSeq.end(); ++pIter)
		{
			unsigned short pLostSeqId = *pIter;
			unsigned short lostSeq = pLostSeqId;
			if (lostSeq == seq)
				return;
		}
		m_lostSeq.push_back(seq);
	}
private:
	std::list<unsigned short>	m_lostSeq;
	int m_offset1;
	int m_offset2;
};

class CSotpRtpReliableMsg
{
public:
	tagSotpRtpDataHead m_pRtpDataHead;
	cgcAttachment::pointer m_pAttachment;
	CSotpRtpReliableMsg(const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttachment)
		: m_pAttachment(pAttachment)
	{
		memcpy(&m_pRtpDataHead,&pRtpDataHead,SOTP_RTP_DATA_HEAD_SIZE);
	}
	virtual ~CSotpRtpReliableMsg(void)
	{
		m_pAttachment.reset();
	}
};

class CSotpRtpSource
{
public:
	typedef boost::shared_ptr<CSotpRtpSource> pointer;
	static CSotpRtpSource::pointer create(cgc::bigint nRoomId,cgc::bigint nSrcId)
	{
		return CSotpRtpSource::pointer(new CSotpRtpSource(nRoomId,nSrcId));
	}
	cgc::bigint GetRoomId(void) const {return m_nRoomId;}
	cgc::bigint GetSrcId(void) const {return m_nSrcId;}
	void SetLastTime(time_t v = time(0)) {m_tLastTime = v;}
	time_t GetLastTime(void) const {return m_tLastTime;}

	void SetRemote(const cgcRemote::pointer& v) {m_pRemote = v;}
	const cgcRemote::pointer& GetRemote(void) const {return m_pRemote;}

	//void AddSinkSend(cgc::bigint nToId);
	//void DelSinkSend(cgc::bigint nToId);
	//bool IsSinkSend(cgc::bigint nToId) const;
	//const CLockMap<cgc::bigint,bool>& GetSinkSendList(void) const {return m_pSinkSendList;}
	// register sink
	void AddSinkRecv(cgc::bigint nDestId);
	void DelSinkRecv(cgc::bigint nDestId);
	bool IsSinkRecv(cgc::bigint nDestId) const;
	const CLockMap<cgc::bigint,bool>& GetSinkRecvList(void) const {return m_pSinkRecvList;}
	void ClearSinkRecv(bool bLock);

	void CaculateMissedPackets(unsigned short nSeq,cgc::uint8 nNAKType,const cgcRemote::pointer& pcgcRemote);
	void UpdateReliableQueue(const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttackment);
	void PushRtpData(const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttackment);
	void GetWholeFrame(HSotpRtpFrameCallback pCallback, void* nUserData);
	void SendReliableMsg(unsigned short nStartSeq,unsigned short nEndSeq,const cgcRemote::pointer& pcgcRemote);
	void SendRegisterSink(const cgcRemote::pointer& pcgcRemote);

	unsigned short GetNextSeq(void) {return ++m_nCurrentSeq;}

	CSotpRtpSource(cgc::bigint nRoomId,cgc::bigint nSrcId);
	virtual ~CSotpRtpSource(void);
private:
	void sendNAKRequest(unsigned short nSeq, unsigned short nCount,const cgcRemote::pointer& pcgcRemote);

private:
	cgc::bigint m_nRoomId;
	cgc::bigint m_nSrcId;
	time_t m_tLastTime;
	cgcRemote::pointer m_pRemote;
	CLockMap<cgc::bigint,bool> m_pSinkSendList;
	CLockMap<cgc::bigint,bool> m_pSinkRecvList;

	cgc::uint32  m_nLastFrameTimestamp;
	int m_nWaitForFrameSeq;
	bool m_bWaitforNextKeyVideo;
	cgc::uint16 m_nLostData;
	CLockMap<cgc::uint32,CSotpRtpFrame::pointer> m_pReceiveFrames;	// ts->

	boost::mutex m_pRelialeMutex;
	CSotpRtpReliableMsg*   m_pReliableQueue[RELIABLE_QUEUE_SIZE];
	CLostSeqInfo theLostSeqInfo1;
	CLostSeqInfo theLostSeqInfo2;
	int m_nLastPacketSeq;

	unsigned short m_nCurrentSeq;
};
const CSotpRtpSource::pointer NullSotpRtpSource;

} // cgc namespace

#endif // __SotpRtpSource_h__
