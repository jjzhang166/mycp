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
#include "SotpRtpSession.h"

namespace cgc
{

CSotpRtpSession::CSotpRtpSession(bool bServerMode)
: m_bServerMode(bServerMode)
, m_pCallback(NULL), m_nCbUserData(0)

{
}
CSotpRtpSession::~CSotpRtpSession(void)
{
}

CSotpRtpRoom::pointer CSotpRtpSession::GetRtpRoom(cgc::bigint nRoomId,bool bCreateNew)
{
	CSotpRtpRoom::pointer pRtpRoom;
	if (!m_pRoomList.find(nRoomId,pRtpRoom))
	{
		if (bCreateNew)
		{
			pRtpRoom = CSotpRtpRoom::create(m_bServerMode,nRoomId);
			CSotpRtpRoom::pointer pRtpRoomTemp;
			m_pRoomList.insert(nRoomId,pRtpRoom,false,&pRtpRoomTemp);
			if (pRtpRoomTemp.get()!=NULL)
			{
				pRtpRoom = pRtpRoomTemp;
			}
		}
	}
	return pRtpRoom;
}
CSotpRtpRoom::pointer CSotpRtpSession::GetRtpRoom(cgc::bigint nRoomId) const
{
	CSotpRtpRoom::pointer pRtpRoom;
	m_pRoomList.find(nRoomId,pRtpRoom);
	return pRtpRoom;
}

bool CSotpRtpSession::doRtpCommand(const tagSotpRtpCommand& pRtpCommand, const cgcRemote::pointer& pcgcRemote, bool bSendRtpCommand)
{
	switch (pRtpCommand.m_nCommand)
	{
	case SOTP_RTP_COMMAND_REGISTER_SOURCE:
		{
			CSotpRtpRoom::pointer pRtpRoom = GetRtpRoom(pRtpCommand.m_nRoomId,true);
			CSotpRtpSource::pointer pRtpSource = pRtpRoom->RegisterSource(pRtpCommand.m_nSrcId,pcgcRemote);
			if (pRtpSource.get()==NULL)
				return false;
		}break;
	case SOTP_RTP_COMMAND_UNREGISTER_SOURCE:
		{
			CSotpRtpRoom::pointer pRtpRoom = GetRtpRoom(pRtpCommand.m_nRoomId,false);
			if (pRtpRoom.get()==NULL)
				return false;
			if (!pRtpRoom->UnRegisterSource(pRtpCommand.m_nSrcId))
				return false;
			if (pRtpRoom->IsEmpty())
				m_pRoomList.remove(pRtpCommand.m_nRoomId);
		}break;
	case SOTP_RTP_COMMAND_REGISTER_SINK:
		{
			CSotpRtpRoom::pointer pRtpRoom = GetRtpRoom(pRtpCommand.m_nRoomId,false);
			if (pRtpRoom.get()==NULL)
				return false;
			if (!pRtpRoom->RegisterSink(pRtpCommand.m_nSrcId,pRtpCommand.u.m_nDestId))
				return false;
			if (!this->m_bServerMode)
			{
				CSotpRtpSource::pointer pRtpSource = pRtpRoom->RegisterSource(pRtpCommand.u.m_nDestId,pcgcRemote);
				if (pRtpSource.get()==NULL)
					return false;
			}
		}break;
	case SOTP_RTP_COMMAND_UNREGISTER_SINK:
		{
			CSotpRtpRoom::pointer pRtpRoom = GetRtpRoom(pRtpCommand.m_nRoomId,false);
			if (pRtpRoom.get()==NULL)
				return false;
			pRtpRoom->UnRegisterSink(pRtpCommand.m_nSrcId,pRtpCommand.u.m_nDestId);
			if (!this->m_bServerMode)
			{
				pRtpRoom->UnRegisterSource(pRtpCommand.u.m_nDestId);
			}
		}break;
	case SOTP_RTP_COMMAND_UNREGISTER_ALLSINK:
		{
			CSotpRtpRoom::pointer pRtpRoom = GetRtpRoom(pRtpCommand.m_nRoomId,false);
			if (pRtpRoom.get()==NULL)
				return false;
			pRtpRoom->UnRegisterAllSink(pRtpCommand.m_nSrcId);
		}break;
	case SOTP_RTP_COMMAND_DATA_REQUEST:
		{
			CSotpRtpRoom::pointer pRtpRoom = GetRtpRoom(pRtpCommand.m_nRoomId,false);
			if (pRtpRoom.get()==NULL)
				return false;

			CSotpRtpSource::pointer pRtpSrcSource = pRtpRoom->GetRtpSource(pRtpCommand.m_nSrcId);
			if (pRtpSrcSource.get()==NULL)
				return false;

			pRtpSrcSource->SendReliableMsg(pRtpCommand.u.m_nDataRequest.m_nSeq-pRtpCommand.u.m_nDataRequest.m_nCount, pRtpCommand.u.m_nDataRequest.m_nSeq,pcgcRemote);
		}break;
	default:
		return false;
	}
	if (!m_bServerMode && bSendRtpCommand && pcgcRemote.get()!=NULL)
	{
		// send rtp command
		size_t nSendSize = 0;
		unsigned char* pSendBuffer = SotpCallTable2::toRtpCommand(pRtpCommand,NULL,nSendSize);
		pcgcRemote->sendData(pSendBuffer,nSendSize);
		delete[] pSendBuffer;
	}
	return true;
}
void CSotpRtpSession::UnRegisterAllRoomSink(cgc::bigint nSrcId)
{
	BoostReadLock rdlock(m_pRoomList.mutex());
	CLockMap<cgc::bigint,CSotpRtpRoom::pointer>::iterator pIterRoom = m_pRoomList.begin();
	for (; pIterRoom!=m_pRoomList.end(); pIterRoom++)
	{
		CSotpRtpRoom::pointer pRtpRoom = pIterRoom->second;
		pRtpRoom->UnRegisterAllSink(nSrcId);
	}
}

bool CSotpRtpSession::doRtpData(const tagSotpRtpDataHead& pRtpDataHead,const cgcAttachment::pointer& pAttackment, const cgcRemote::pointer& pcgcRemote)
{
	if (pAttackment.get()==NULL || !pAttackment->isHasAttach())
		return false;
	CSotpRtpRoom::pointer pRtpRoom = GetRtpRoom(pRtpDataHead.m_nRoomId,false);
	if (pRtpRoom.get()==NULL)
		return false;

	CSotpRtpSource::pointer pRtpSrcSource = pRtpRoom->GetRtpSource(pRtpDataHead.m_nSrcId);
	if (pRtpSrcSource.get()==NULL)
		return false;

	// 
	pRtpSrcSource->CaculateMissedPackets(pRtpDataHead,pcgcRemote);
	if (this->m_bServerMode)
	{
		// save as wait for SOTP_RTP_COMMAND_DATA_REQUEST msg
		pRtpSrcSource->UpdateReliableQueue(pRtpDataHead,pAttackment);
		pRtpRoom->BroadcastRtpData(pRtpDataHead,pAttackment);
	}else
	{
		pRtpSrcSource->PushRtpData(pRtpDataHead,pAttackment);
		pRtpSrcSource->GetWholeFrame(m_pCallback,m_nCbUserData);
	}

	return true;
}

void CSotpRtpSession::CheckRegisterSourceLive(short nExpireSecond)
{
	std::vector<cgc::bigint> pRemoveList;
	{
		const time_t tNow = time(0);
		BoostReadLock rdlock(m_pRoomList.mutex());
		CLockMap<cgc::bigint,CSotpRtpRoom::pointer>::iterator pIterRoom = m_pRoomList.begin();
		for (; pIterRoom!=m_pRoomList.end(); pIterRoom++)
		{
			CSotpRtpRoom::pointer pRtpRoom = pIterRoom->second;
			pRtpRoom->CheckRegisterSourceLive(tNow, nExpireSecond);
			if (pRtpRoom->IsEmpty())
			{
				pRemoveList.push_back(pRtpRoom->GetRoomId());
			}
		}
	}
	for (size_t i=0;i<pRemoveList.size();i++)
	{
		m_pRoomList.remove(pRemoveList[i]);
	}
}

void CSotpRtpSession::CheckRegisterSinkLive(short nExpireSecond, const cgcRemote::pointer& pcgcRemote)
{
	if (!m_bServerMode)
	{
		const time_t tNow = time(0);
		BoostReadLock rdlock(m_pRoomList.mutex());
		CLockMap<cgc::bigint,CSotpRtpRoom::pointer>::iterator pIterRoom = m_pRoomList.begin();
		for (; pIterRoom!=m_pRoomList.end(); pIterRoom++)
		{
			CSotpRtpRoom::pointer pRtpRoom = pIterRoom->second;
			pRtpRoom->CheckRegisterSinkLive(tNow, nExpireSecond, pcgcRemote);
		}
	}
}

void CSotpRtpSession::GetRoomIdList(std::vector<cgc::bigint>& pOutRoomIdList) const
{
	BoostReadLock rdlock(const_cast<boost::shared_mutex&>(m_pRoomList.mutex()));
	CLockMap<cgc::bigint,CSotpRtpRoom::pointer>::const_iterator pIterRoom = m_pRoomList.begin();
	for (; pIterRoom!=m_pRoomList.end(); pIterRoom++)
	{
		CSotpRtpRoom::pointer pRtpRoom = pIterRoom->second;
		pOutRoomIdList.push_back(pRtpRoom->GetRoomId());
	}
}
bool CSotpRtpSession::IsRegisterSource(cgc::bigint nRoomId, cgc::bigint nSrcId) const
{
	CSotpRtpRoom::pointer pRtpRoom = GetRtpRoom(nRoomId);
	if (pRtpRoom.get()==NULL)
		return false;
	return pRtpRoom->IsRegisterSource(nSrcId);
}
bool CSotpRtpSession::IsRegisterSink(cgc::bigint nRoomId, cgc::bigint nSrcId, cgc::bigint nDestId) const
{
	CSotpRtpRoom::pointer pRtpRoom = GetRtpRoom(nRoomId);
	if (pRtpRoom.get()==NULL)
		return false;

	CSotpRtpSource::pointer pRtpSrcSource = pRtpRoom->GetRtpSource(nSrcId);
	if (pRtpSrcSource.get()==NULL)
		return false;

	return pRtpSrcSource->IsSinkRecv(nDestId);
}


} // cgc namespace
