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

#include "../CGCBase/cgcdef.h"
#include "../CGCClass/SotpCallTable2.h"
#include "SotpRtpSource.h"

namespace cgc
{
#ifdef WIN32
#include <Windows.h>
#include <Mmsystem.h>
#pragma comment(lib, "winmm.lib")
#else
#include <time.h>   
inline unsigned long timeGetTime()  
{  
	unsigned long uptime = 0;  
	struct timespec on;  
	if(clock_gettime(CLOCK_MONOTONIC, &on) == 0)  
		uptime = on.tv_sec*1000 + on.tv_nsec/1000000;  
	return uptime;  
} 
#endif


CSotpRtpSource::CSotpRtpSource(cgc::bigint nRoomId,cgc::bigint nSrcId)
: m_nRoomId(nRoomId), m_nSrcId(nSrcId)
, m_tLastTime(0)
, m_nLastFrameTimestamp(0), m_nWaitForFrameSeq(-1), m_bWaitforNextKeyVideo(false), m_nLostData(0)
, theLostSeqInfo1(1,6), theLostSeqInfo2(7,25)//, theLostSeqInfo3(13,20)
, m_nLastPacketSeq(-1)
, m_nCurrentSeq(0)

{
	m_tLastTime = time(0);
	memset(m_pReliableQueue,0,sizeof(m_pReliableQueue));
	//const cgcAttachment::pointer NullcgcAttachment;
	//for(int i=0; i< RELIABLE_QUEUE_SIZE; ++i)
	//{
	//	m_pReliableQueue[i] = NullcgcAttachment;
	//}
}
CSotpRtpSource::~CSotpRtpSource(void)
{
	{
		boost::mutex::scoped_lock lock(m_pRelialeMutex);
		for(int i=0; i< RELIABLE_QUEUE_SIZE; ++i)
		{
			if (m_pReliableQueue[i] != 0)
			{
				delete m_pReliableQueue[i];
				m_pReliableQueue[i] = NULL;
			}
		}
	}
}

//void CSotpRtpSource::AddSinkSend(cgc::bigint nToId)
//{
//	m_pSinkSendList.insert(nToId,true);
//}
//void CSotpRtpSource::DelSinkSend(cgc::bigint nToId)
//{
//	m_pSinkSendList.remove(nToId);
//}
//bool CSotpRtpSource::IsSinkSend(cgc::bigint nToId) const
//{
//	return m_pSinkSendList.exist(nToId);
//}

void CSotpRtpSource::AddSinkRecv(cgc::bigint nDestId)
{
	m_pSinkRecvList.insert(nDestId,true);
}
void CSotpRtpSource::DelSinkRecv(cgc::bigint nDestId)
{
	m_pSinkRecvList.remove(nDestId);
}
bool CSotpRtpSource::IsSinkRecv(cgc::bigint nDestId) const
{
	return m_pSinkRecvList.exist(nDestId);
}
void CSotpRtpSource::ClearSinkRecv(bool bLock)
{
	m_pSinkRecvList.clear(bLock);
}

void CSotpRtpSource::CaculateMissedPackets(const tagSotpRtpDataHead& pRtpDataHead,const cgcRemote::pointer& pcgcRemote)
{
	const cgc::uint16 nSeq = pRtpDataHead.m_nSeq;
	const cgc::uint8 nNAKType = pRtpDataHead.m_nNAKType;
	if (nNAKType==SOTP_RTP_NAK_REQUEST_2)
	{
		unsigned short sendSeq = 0;
		int sendCount = 0;
		bool bSendNAKREquest = false;
		if (theLostSeqInfo1.recv(nSeq, m_nLastPacketSeq, sendSeq, sendCount, !bSendNAKREquest))
		{
			bSendNAKREquest = true;
			sendNAKRequest(sendSeq+sendCount, sendCount, pcgcRemote);
		}
		if (theLostSeqInfo2.recv(nSeq, m_nLastPacketSeq, sendSeq, sendCount, !bSendNAKREquest))
		{
			bSendNAKREquest = true;
			sendNAKRequest(sendSeq+sendCount, sendCount, pcgcRemote);
		}
		//if (theLostSeqInfo3.recv(nSeq, m_nLastPacketSeq, sendSeq, sendCount, !bSendNAKREquest))
		//{
		//	bSendNAKREquest = true;
		//	sendNAKRequest(sendSeq+sendCount, sendCount, pcgcRemote);
		//}
	}

	const unsigned short expectSeq = m_nLastPacketSeq + 1;
	if (nSeq == expectSeq || m_nLastPacketSeq==-1) {
		m_nLastPacketSeq = nSeq;
		return;
	}

	// 计算相距多少个SEQ
	int intervalseqs = nSeq - m_nLastPacketSeq;
	if (intervalseqs<0)
		intervalseqs = (intervalseqs*-1)-1;
	else if (intervalseqs>0)
		intervalseqs--;

	// **效果好很多，就不知流量如何；
	const cgc::uint16 nMaxResendCount = (pRtpDataHead.m_nTotleLength/pRtpDataHead.m_nUnitLength)>10?(MAX_RESEND_COUNT+3):MAX_RESEND_COUNT;
	//const cgc::uint16 nMaxResendCount = MAX_RESEND_COUNT;
	int n=0;
	if (nSeq>expectSeq && (nSeq-expectSeq) < 0x7F00)	// 正常大小SEQ，缺少前面SEQ
	{
		n = ((nSeq-expectSeq) > nMaxResendCount) ? nMaxResendCount : (nSeq-expectSeq);
		m_nLastPacketSeq = nSeq;

		if (nNAKType==SOTP_RTP_NAK_REQUEST_2)
		{
			if (intervalseqs >= nMaxResendCount)
			{
				theLostSeqInfo1.clear();
				theLostSeqInfo2.clear();
				//theLostSeqInfo3.clear();
				intervalseqs = 0;	// 过期数据重发也无用,用于网络特别差时减少流量
				//intervalseqs = nMaxResendCount;	// 过期数据重发也无用,用于网络特别差时减少流量
			}
			if (intervalseqs>0)
			{
				theLostSeqInfo1.lost(nSeq-intervalseqs, intervalseqs);
				theLostSeqInfo2.lost(nSeq-intervalseqs, intervalseqs);
				//theLostSeqInfo3.lost(nSeq-intervalseqs, intervalseqs);
			}
		}
	}
	else if (expectSeq - nSeq> 0x7F00)	// seq 重新从头开始计算
	//else if (expectSeq > nSeq && (intervalseqs > 200))
	{
		const int diff = nSeq + (65535 - expectSeq);
		n = (diff > nMaxResendCount) ? nMaxResendCount : diff;
		m_nLastPacketSeq = nSeq;

		if (nNAKType==SOTP_RTP_NAK_REQUEST_2)
		{
			// 先发送65535前面
			int diff1 = 65535 - expectSeq;
			if (diff1 > nMaxResendCount)
				diff1 = nMaxResendCount;
			theLostSeqInfo1.lost(65535-diff1, diff1+1);
			theLostSeqInfo2.lost(65535-diff1, diff1+1);
			//theLostSeqInfo3.lost(65535-diff1, diff1+1);

			// 发送65535后,0开始数据
			const int diff2 = nSeq>0?(nSeq-1):0;
			if (diff2 >= nMaxResendCount)
			{
				theLostSeqInfo1.clear();
				theLostSeqInfo2.clear();
				//theLostSeqInfo3.clear();
				//diff2 = 0;	// 过期数据重发也无用,用于网络特别差时减少流量
			}else if (diff2 > 0)
			{
				theLostSeqInfo1.lost(nSeq-diff2, diff2);
				theLostSeqInfo2.lost(nSeq-diff2, diff2);
				//theLostSeqInfo3.lost(nSeq-diff2, diff2);
			}

			//if (intervalseqs > nMaxResendCount)
			//	intervalseqs = nMaxResendCount;	// 过期数据重发也无用,用于网络特别差时减少流量

			//theLostSeqInfo1.lost(seq-intervalseqs, intervalseqs);
			//theLostSeqInfo2.lost(seq-intervalseqs, intervalseqs);
			//theLostSeqInfo3.lost(seq-intervalseqs, intervalseqs);
		}
	}

	if (nNAKType==SOTP_RTP_NAK_REQUEST_1 && n>0)	// *音频 感觉网络差时,音频效果好一些
	{
		sendNAKRequest(nSeq, n,pcgcRemote);
	}

}
void CSotpRtpSource::sendNAKRequest(unsigned short nSeq, unsigned short nCount,const cgcRemote::pointer& pcgcRemote)
{
	tagSotpRtpCommand pCommand;
	pCommand.m_nCommand = SOTP_RTP_COMMAND_DATA_REQUEST;
	pCommand.m_nRoomId = this->GetRoomId();
	pCommand.m_nSrcId = this->GetSrcId();
	pCommand.u.m_nDataRequest.m_nSeq = nSeq;
	pCommand.u.m_nDataRequest.m_nCount = nCount;
	size_t nSendSize = 0;
	unsigned char lpszBuffer[64];	// SOTP_RTP_COMMAND_SIZE=25
	SotpCallTable2::toRtpCommand(pCommand,lpszBuffer,nSendSize);
	pcgcRemote->sendData((const unsigned char*)lpszBuffer,nSendSize);
}
void CSotpRtpSource::UpdateReliableQueue(const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttackment)
{
	const unsigned short nSeq = pRtpDataHead.m_nSeq;
	const int i = nSeq%RELIABLE_QUEUE_SIZE;
	boost::mutex::scoped_lock lock(m_pRelialeMutex);
	if (m_pReliableQueue[i] != 0)
	{
		delete m_pReliableQueue[i];
	}
	m_pReliableQueue[i] = new CSotpRtpReliableMsg(pRtpDataHead,pAttackment);
}
void CSotpRtpSource::PushRtpData(const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttackment)
{
	if (pRtpDataHead.m_nIndex>=SOTP_RTP_MAX_PACKETS_PER_FRAME) return;
	const cgc::uint32 ts = pRtpDataHead.m_nTimestamp;
	// the packet expire time.
	if (ts<m_nLastFrameTimestamp && (m_nLastFrameTimestamp-ts) < 0xFFFF)		// 前面过期数据
	{
		return;
	}else if (ts>m_nLastFrameTimestamp && (ts-m_nLastFrameTimestamp)>20*1000)	// 20S，中间停止后，后面继续
	{
		m_nLastPacketSeq = -1;
	}

	CSotpRtpFrame::pointer pFrame;
	if (!m_pReceiveFrames.find(ts,pFrame))
	{
		pFrame = CSotpRtpFrame::create(pRtpDataHead);
		pFrame->m_pPayload = new char[pRtpDataHead.m_nTotleLength+1];
		if (pFrame->m_pPayload==0)
		{
			return;
		}
		memset(pFrame->m_pPayload, 0, pRtpDataHead.m_nTotleLength+1);
		memcpy(&pFrame->m_pRtpHead,&pRtpDataHead,SOTP_RTP_DATA_HEAD_SIZE);
		pFrame->m_nFirstSeq = pRtpDataHead.m_nSeq-pRtpDataHead.m_nIndex;	// *seq重头开始也正常
		pFrame->m_nPacketNumber = (pRtpDataHead.m_nTotleLength+pRtpDataHead.m_nUnitLength-1)/pRtpDataHead.m_nUnitLength;
		if (pRtpDataHead.m_nTotleLength>20*1024 || pRtpDataHead.m_nDataType==SOTP_RTP_NAK_DATA_VIDEO_I || pRtpDataHead.m_nDataType==SOTP_RTP_NAK_DATA_VIDEO)
			pFrame->m_nExpireTime = timeGetTime()+1800;
		else
			pFrame->m_nExpireTime = timeGetTime()+800;
		CSotpRtpFrame::pointer pFromTemp;
		m_pReceiveFrames.insert(ts,pFrame,false,&pFromTemp);
		if (pFromTemp!=NULL)
		{
			pFrame = pFromTemp;
		}
	}

	if (m_nLastFrameTimestamp == 0)
	{
		// 不能删除，用于解决中间进房间出视频慢问题
		m_nLastFrameTimestamp = pFrame->m_pRtpHead.m_nTimestamp - 1;
		m_nWaitForFrameSeq = (int)(unsigned short)(pFrame->m_nFirstSeq);
	}

	//fill frame data.
	if (pFrame->m_nFilled[pRtpDataHead.m_nIndex] != 1)
	{
		const cgc::uint16 nDataOffset = pRtpDataHead.m_nIndex*pRtpDataHead.m_nUnitLength;
		const cgc::uint16 nDataLength = (pRtpDataHead.m_nTotleLength-nDataOffset)>=pRtpDataHead.m_nUnitLength?pRtpDataHead.m_nUnitLength:(pRtpDataHead.m_nTotleLength-nDataOffset);
		if (nDataLength==pAttackment->getAttachSize())
		{
			pFrame->m_nFilled[pRtpDataHead.m_nIndex] = 1;
			memcpy((char*)pFrame->m_pPayload + nDataOffset,(const unsigned char*)pAttackment->getAttachData(),nDataLength);
		}
	}
}

inline bool IsWholeFrame(const CSotpRtpFrame::pointer& frame)
{
	cgc::uint8 i = 0;
	for(; i < frame->m_nPacketNumber; i++) {
		if(frame->m_nFilled[i] != 1) {
			return false;
		}
	}
	return true;
}

void CSotpRtpSource::GetWholeFrame(HSotpRtpFrameCallback pCallback, void* nUserData)
{
	cgc::uint16 nCount = 0;
	const cgc::uint32 tNow = timeGetTime();
	BoostWriteLock wrlock(m_pReceiveFrames.mutex());
	CLockMap<cgc::uint32,CSotpRtpFrame::pointer>::iterator pIter = m_pReceiveFrames.begin();
	for (; pIter!=m_pReceiveFrames.end(); pIter++)
	{
		const CSotpRtpFrame::pointer pFrame = pIter->second;
		if (IsWholeFrame(pFrame) && (m_nWaitForFrameSeq == -1 || ((unsigned short)(m_nWaitForFrameSeq)) == pFrame->m_nFirstSeq))
		{
			// OK
			m_pReceiveFrames.erase(pIter);
			m_nLastFrameTimestamp = pFrame->m_pRtpHead.m_nTimestamp;
			m_nWaitForFrameSeq = (int)(cgc::uint16)(pFrame->m_nFirstSeq + pFrame->m_nPacketNumber);
			if (m_bWaitforNextKeyVideo && pFrame->m_pRtpHead.m_nDataType==SOTP_RTP_NAK_DATA_VIDEO_I)
				m_bWaitforNextKeyVideo = false;

			if (!m_bWaitforNextKeyVideo || pFrame->m_pRtpHead.m_nDataType != SOTP_RTP_NAK_DATA_VIDEO)
			{
				//callback the frame.
				const cgc::uint16 nLostDataTemp = m_nLostData;
				m_nLostData = 0;
				wrlock.unlock();	// **
				if(pCallback!=0)
					pCallback(this->GetSrcId(), pFrame, nLostDataTemp, nUserData);
			}
			break;
		}else if (pFrame->m_nExpireTime < tNow)
		{
			// expire time
			m_pReceiveFrames.erase(pIter);
			m_nLastFrameTimestamp = pFrame->m_pRtpHead.m_nTimestamp;
			m_nWaitForFrameSeq = (int)(cgc::uint16)(pFrame->m_nFirstSeq + pFrame->m_nPacketNumber);

			if (!m_bWaitforNextKeyVideo && IsWholeFrame(pFrame))
			{
				//callback the frame.
				const cgc::uint16 nLostDataTemp = m_nLostData;
				m_nLostData = 0;
				wrlock.unlock();	// **
				if(pCallback!=0)
					pCallback(this->GetSrcId(), pFrame, nLostDataTemp, nUserData);
			}else
			{
				m_nLostData++;
			}
			m_bWaitforNextKeyVideo = (pFrame->m_pRtpHead.m_nDataType==SOTP_RTP_NAK_DATA_VIDEO_I||pFrame->m_pRtpHead.m_nDataType==SOTP_RTP_NAK_DATA_VIDEO)?true:false;
			break;
		}else if ((nCount++)>=3)
		{
			break;
		}
	}
}


void CSotpRtpSource::SendReliableMsg(unsigned short nStartSeq,unsigned short nEndSeq,const cgcRemote::pointer& pcgcRemote)
{
	unsigned char * pSendBuffer = NULL;
	size_t nBufferSize = 0;

	int i = 0;
	for (int k=nStartSeq; k<=nEndSeq; ++k)
	{
		i= k%RELIABLE_QUEUE_SIZE;
		boost::mutex::scoped_lock lock(m_pRelialeMutex);
		const CSotpRtpReliableMsg* pRtpReliableMsg = m_pReliableQueue[i];
		if (pRtpReliableMsg!=NULL)
		{
			const size_t nSizeTemp = 20+SOTP_RTP_DATA_HEAD_SIZE+pRtpReliableMsg->m_pAttachment->getAttachSize();
			if (pSendBuffer==NULL)
			{
				nBufferSize = nSizeTemp;
				pSendBuffer = new unsigned char[nBufferSize];
			}else if (nSizeTemp>nBufferSize)
			{
				delete[] pSendBuffer;
				nBufferSize = nSizeTemp;
				pSendBuffer = new unsigned char[nBufferSize];
			}
			size_t nSendSize = 0;
			SotpCallTable2::toRtpData(pRtpReliableMsg->m_pRtpDataHead,pRtpReliableMsg->m_pAttachment,pSendBuffer,nSendSize);
			pcgcRemote->sendData(pSendBuffer,nSendSize);
		}
	}
	if (pSendBuffer!=NULL)
		delete[] pSendBuffer;
}
void CSotpRtpSource::SendRegisterSink(const cgcRemote::pointer& pcgcRemote)
{
	if (pcgcRemote.get()==NULL)
		return;
	unsigned char lpszBuffer[64];	// SOTP_RTP_COMMAND_SIZE=25
	tagSotpRtpCommand pRtpCommand;
	pRtpCommand.m_nCommand = SOTP_RTP_COMMAND_REGISTER_SINK;
	pRtpCommand.m_nRoomId = this->GetRoomId();
	pRtpCommand.m_nSrcId = this->GetSrcId();

	BoostReadLock rdlock(const_cast<boost::shared_mutex&>(m_pSinkRecvList.mutex()));
	CLockMap<cgc::bigint,bool>::const_iterator pIter = m_pSinkRecvList.begin();
	for (; pIter!=m_pSinkRecvList.end(); pIter++)
	{
		const cgc::bigint nDestId = pIter->first;
		pRtpCommand.u.m_nDestId = nDestId;

		size_t nSendSize = 0;
		SotpCallTable2::toRtpCommand(pRtpCommand,lpszBuffer,nSendSize);
		pcgcRemote->sendData(lpszBuffer,nSendSize);
	}

}


} // cgc namespace
