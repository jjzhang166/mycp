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
#include "SotpRtpRoom.h"

namespace cgc
{

CSotpRtpRoom::CSotpRtpRoom(bool bServerMode,cgc::bigint nRoomId)
: m_bServerMode(bServerMode), m_nRoomId(nRoomId)

{
	printf("**** CSotpRtpRoom() roomid=%lld\n",m_nRoomId);
}
CSotpRtpRoom::~CSotpRtpRoom(void)
{
	printf("**** ~CSotpRtpRoom() roomid=%lld\n",m_nRoomId);
}

CSotpRtpSource::pointer CSotpRtpRoom::RegisterSource(cgc::bigint nSrcId, const cgcRemote::pointer& pcgcRemote)
{
	if (nSrcId==0)
		return NullSotpRtpSource;

	CSotpRtpSource::pointer pRtpSource;
	if (!m_pSourceList.find(nSrcId,pRtpSource))
	{
		pRtpSource = CSotpRtpSource::create(m_nRoomId,nSrcId);
		CSotpRtpSource::pointer pRtpSourceTemp;
		m_pSourceList.insert(nSrcId,pRtpSource,false,&pRtpSourceTemp);
		if (pRtpSourceTemp.get()!=NULL)
		{
			pRtpSource = pRtpSourceTemp;
		}
	}else
	{
		pRtpSource->SetLastTime();
	}
	pRtpSource->SetRemote(pcgcRemote);
	return pRtpSource;
}
bool CSotpRtpRoom::UnRegisterSource(cgc::bigint nSrcId)
{
	return m_pSourceList.remove(nSrcId);
}
bool CSotpRtpRoom::RegisterSink(cgc::bigint nSrcId,cgc::bigint nDestId)
{
	CSotpRtpSource::pointer pRtpSrcSource;
	if (!m_pSourceList.find(nSrcId,pRtpSrcSource))
		return false;

	//if (m_bServerMode)
	//{
	//	CSotpRtpSource::pointer pRtpDestSource;
	//	if (!m_pSourceList.find(nDestId,pRtpDestSource))
	//		return false;
	//	pRtpSrcSource->AddSinkRecv(nDestId);
	//	pRtpDestSource->AddSinkSend(nSrcId);
	//}else
	{
		pRtpSrcSource->AddSinkRecv(nDestId);
	}
	return true;
}
void CSotpRtpRoom::UnRegisterSink(cgc::bigint nSrcId,cgc::bigint nDestId)
{
	CSotpRtpSource::pointer pRtpSrcSource;
	if (m_pSourceList.find(nSrcId,pRtpSrcSource))
		pRtpSrcSource->DelSinkRecv(nDestId);

	//CSotpRtpSource::pointer pRtpDestSource;
	//if (m_pSourceList.find(nDestId,pRtpDestSource))
	//	pRtpDestSource->DelSinkSend(nSrcId);
}
void CSotpRtpRoom::UnRegisterAllSink(cgc::bigint nSrcId)
{
	CSotpRtpSource::pointer pRtpSrcSource;
	if (m_pSourceList.find(nSrcId,pRtpSrcSource))
	{
		UnRegisterAllSink(pRtpSrcSource);
	}
}
void CSotpRtpRoom::UnRegisterAllSink(const CSotpRtpSource::pointer& pRtpSrcSource)
{
	{
		//const CLockMap<cgc::bigint,bool>& pList = pRtpSrcSource->GetSinkRecvList();
		//BoostReadLock rdlock(const_cast<boost::shared_mutex&>(pList.mutex()));
		//CLockMap<cgc::bigint,bool>::const_iterator pIter = pList.begin();
		//for (; pIter!=pList.end(); pIter++)
		//{
		//	const cgc::bigint nDestId = pIter->first;
		//	CSotpRtpSource::pointer pRtpDestSource;
		//	if (m_pSourceList.find(nDestId,pRtpDestSource))
		//		pRtpDestSource->DelSinkSend(pRtpSrcSource->GetSrcId());
		//}
		//pRtpSrcSource->ClearSinkRecv(false);
		pRtpSrcSource->ClearSinkRecv(true);
	}
}

CSotpRtpSource::pointer CSotpRtpRoom::GetRtpSource(cgc::bigint nSrcId) const
{
	CSotpRtpSource::pointer pRtpSource;
	m_pSourceList.find(nSrcId,pRtpSource);
	return pRtpSource;
}
bool CSotpRtpRoom::IsRegisterSource(cgc::bigint nSrcId) const
{
	return m_pSourceList.exist(nSrcId);
}

void CSotpRtpRoom::BroadcastRtpData(const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttackment) const
{
	{
		unsigned char * pSendBuffer = NULL;
		size_t nSendSize = 0;
		BoostReadLock rdlock(const_cast<boost::shared_mutex&>(m_pSourceList.mutex()));
		CLockMap<cgc::bigint,CSotpRtpSource::pointer>::const_iterator pIter = m_pSourceList.begin();
		for (; pIter!=m_pSourceList.end(); pIter++)
		{
			const cgc::bigint nSrcId = pIter->first;
			if (nSrcId==pRtpDataHead.m_nSrcId)
				continue;
			CSotpRtpSource::pointer pRtpDestSource = pIter->second;
			if (!pRtpDestSource->IsSinkRecv(pRtpDataHead.m_nSrcId))
				continue;
			const cgcRemote::pointer& pcgcRemote = pRtpDestSource->GetRemote();
			if (pcgcRemote.get()!=NULL)
			{
				if (pSendBuffer==NULL)
				{
					pSendBuffer = new unsigned char[20+SOTP_RTP_DATA_HEAD_SIZE+pAttackment->getAttachSize()];
					SotpCallTable2::toRtpData(pRtpDataHead,pAttackment,pSendBuffer,nSendSize);
				}
				pcgcRemote->sendData(pSendBuffer, nSendSize);
			}
		}
		if (pSendBuffer!=NULL)
			delete[] pSendBuffer;
	}
}
void CSotpRtpRoom::CheckRegisterSourceLive(time_t tNow,short nExpireSecond)
{
	std::vector<cgc::bigint> pRemoveList;
	{
		BoostReadLock rdlock(m_pSourceList.mutex());
		CLockMap<cgc::bigint,CSotpRtpSource::pointer>::iterator pIter = m_pSourceList.begin();
		for (; pIter!=m_pSourceList.end(); pIter++)
		{
			CSotpRtpSource::pointer pRtpSrcSource = pIter->second;
			if (tNow-pRtpSrcSource->GetLastTime()>nExpireSecond)
			{
				UnRegisterAllSink(pRtpSrcSource);
				pRemoveList.push_back(pRtpSrcSource->GetSrcId());
			}
		}
	}
	for (size_t i=0;i<pRemoveList.size();i++)
	{
		m_pSourceList.remove(pRemoveList[i]);
	}
}
void CSotpRtpRoom::CheckRegisterSinkLive(time_t tNow,short nExpireSecond,const cgcRemote::pointer& pcgcRemote)
{
	//bool bCallbackRoomOnly = false;
	if (!m_bServerMode)
	{
		unsigned char pSendBuffer[64];
		tagSotpRtpCommand pRtpCommand;
		pRtpCommand.m_nCommand = SOTP_RTP_COMMAND_REGISTER_SOURCE;
		pRtpCommand.m_nRoomId = this->GetRoomId();
		BoostReadLock rdlock(m_pSourceList.mutex());
		CLockMap<cgc::bigint,CSotpRtpSource::pointer>::iterator pIter = m_pSourceList.begin();
		for (; pIter!=m_pSourceList.end(); pIter++)
		{
			CSotpRtpSource::pointer pRtpSrcSource = pIter->second;
			if (tNow-pRtpSrcSource->GetLastTime()>nExpireSecond)
			{
				if (pcgcRemote.get()!=NULL)
				{
					pRtpCommand.m_nSrcId = pRtpSrcSource->GetSrcId();
					size_t nSendSize = 0;
					SotpCallTable2::toRtpCommand(pRtpCommand,pSendBuffer,nSendSize);
					pcgcRemote->sendData(pSendBuffer,nSendSize);

					// **
					pRtpSrcSource->SendRegisterSink(pcgcRemote);
				}
			}
		}
	}

}


} // cgc namespace
