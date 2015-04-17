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

CSotpRtpSource::CSotpRtpSource(cgc::bigint nRoomId,cgc::bigint nSrcId)
: m_nRoomId(nRoomId), m_nSrcId(nSrcId)
, m_tLastTime(0)
, theLostSeqInfo1(1,6), theLostSeqInfo2(7,25)//, theLostSeqInfo3(13,20)
, lastSequenceOfRecvpacket_(-1)
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

void CSotpRtpSource::AddSinkSend(cgc::bigint nToId)
{
	m_pSinkSendList.insert(nToId,true);
}
void CSotpRtpSource::DelSinkSend(cgc::bigint nToId)
{
	m_pSinkSendList.remove(nToId);
}
bool CSotpRtpSource::IsSinkSend(cgc::bigint nToId) const
{
	return m_pSinkSendList.exist(nToId);
}

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

void CSotpRtpSource::CaculateMissedPackets(unsigned short nSeq,unsigned char nNAKType,const cgcRemote::pointer& pcgcRemote)
{
	if (nNAKType==2)
	{
		unsigned short sendSeq = 0;
		int sendCount = 0;
		bool bSendNAKREquest = false;
		if (theLostSeqInfo1.recv(nSeq, lastSequenceOfRecvpacket_, sendSeq, sendCount, !bSendNAKREquest))
		{
			bSendNAKREquest = true;
			sendNAKRequest(sendSeq+sendCount, sendCount, pcgcRemote);
		}
		if (theLostSeqInfo2.recv(nSeq, lastSequenceOfRecvpacket_, sendSeq, sendCount, !bSendNAKREquest))
		{
			bSendNAKREquest = true;
			sendNAKRequest(sendSeq+sendCount, sendCount, pcgcRemote);
		}
		//if (theLostSeqInfo3.recv(nSeq, lastSequenceOfRecvpacket_, sendSeq, sendCount, !bSendNAKREquest))
		//{
		//	bSendNAKREquest = true;
		//	sendNAKRequest(sendSeq+sendCount, sendCount, pcgcRemote);
		//}
	}

	const unsigned short expectSeq = lastSequenceOfRecvpacket_ + 1;
	if (nSeq == expectSeq || lastSequenceOfRecvpacket_==-1) {
		lastSequenceOfRecvpacket_ = nSeq;
		return;
	}

	// 计算相距多少个SEQ
	int intervalseqs = nSeq - lastSequenceOfRecvpacket_;
	if (intervalseqs<0)
		intervalseqs = (intervalseqs*-1)-1;
	else if (intervalseqs>0)
		intervalseqs--;

	int n=0;
	if (nSeq>expectSeq && (nSeq-expectSeq) < 0x7F00)	// 正常大小SEQ，缺少前面SEQ
	{
		n = ((nSeq-expectSeq) > MAX_RESEND_COUNT) ? MAX_RESEND_COUNT : (nSeq-expectSeq);
		lastSequenceOfRecvpacket_ = nSeq;

		if (nNAKType==2)
		{
			if (intervalseqs >= MAX_RESEND_COUNT)
			{
				theLostSeqInfo1.clear();
				theLostSeqInfo2.clear();
				//theLostSeqInfo3.clear();
				intervalseqs = 0;	// 过期数据重发也无用,用于网络特别差时减少流量
				//intervalseqs = MAX_RESEND_COUNT;	// 过期数据重发也无用,用于网络特别差时减少流量
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
		n = (diff > MAX_RESEND_COUNT) ? MAX_RESEND_COUNT : diff;
		lastSequenceOfRecvpacket_ = nSeq;

		if (nNAKType==2)
		{
			// 先发送65535前面
			int diff1 = 65535 - expectSeq;
			if (diff1 > MAX_RESEND_COUNT)
				diff1 = MAX_RESEND_COUNT;
			theLostSeqInfo1.lost(65535-diff1, diff1+1);
			theLostSeqInfo2.lost(65535-diff1, diff1+1);
			//theLostSeqInfo3.lost(65535-diff1, diff1+1);

			// 发送65535后,0开始数据
			const int diff2 = nSeq>0?(nSeq-1):0;
			if (diff2 >= MAX_RESEND_COUNT)
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

			//if (intervalseqs > MAX_RESEND_COUNT)
			//	intervalseqs = MAX_RESEND_COUNT;	// 过期数据重发也无用,用于网络特别差时减少流量

			//theLostSeqInfo1.lost(seq-intervalseqs, intervalseqs);
			//theLostSeqInfo2.lost(seq-intervalseqs, intervalseqs);
			//theLostSeqInfo3.lost(seq-intervalseqs, intervalseqs);
		}
	}

	if (nNAKType==1 && n>0)	// *音频 感觉网络差时,音频效果好一些
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
	//char lpszBuffer[64];	// SOTP_RTP_COMMAND_SIZE=25
	//int nSendSize = sprintf(lpszBuffer,"4 SOTP/2.1\nD");
	//tagSotpRtpCommand pCommand;
	//pCommand.m_nCommand = SOTP_RTP_COMMAND_DATA_REQUEST;
	//pCommand.m_nRoomId = this->GetRoomId();
	//pCommand.m_nSrcId = this->GetSrcId();
	//pCommand.u.m_nDataRequest.m_nSeq = nSeq;
	//pCommand.u.m_nDataRequest.m_nCount = nCount;
	//memcpy(lpszBuffer+nSendSize,&pCommand,SOTP_RTP_COMMAND_SIZE);
	//nSendSize += (SOTP_RTP_COMMAND_SIZE+1);
	//lpszBuffer[nSendSize-1] = '\n';
	pcgcRemote->sendData((const unsigned char*)lpszBuffer,nSendSize);
}
void CSotpRtpSource::UpdateReliableQueue(unsigned short nSeq,const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttackment)
{
	const int i = nSeq%RELIABLE_QUEUE_SIZE;
	boost::mutex::scoped_lock lock(m_pRelialeMutex);
	if (m_pReliableQueue[i] != 0)
	{
		delete m_pReliableQueue[i];
	}
	m_pReliableQueue[i] = new CSotpRtpReliableMsg(pRtpDataHead,pAttackment);
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
			//size_t nSendSize = sprintf((char*)pSendBuffer,"4 SOTP/2.1\nE");
			//memcpy(pSendBuffer+nSendSize,&pRtpReliableMsg->m_pRtpDataHead,SOTP_RTP_DATA_HEAD_SIZE);
			//nSendSize += (SOTP_RTP_DATA_HEAD_SIZE);
			//memcpy(pSendBuffer+nSendSize,(const void*)(const unsigned char *)pRtpReliableMsg->m_pAttachment->getAttachData(),pRtpReliableMsg->m_pAttachment->getAttachSize());
			//nSendSize += (pRtpReliableMsg->m_pAttachment->getAttachSize()+1);
			//pSendBuffer[nSendSize-1] = '\n';
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
