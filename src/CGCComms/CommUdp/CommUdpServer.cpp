/*
    MYCP is a HTTP and C++ Web Application Server.
	CommUdpServer is a UDP socket server.
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

// CommUdpServer.cpp : Defines the exported functions for the DLL application.
//

//////////////////////////////////////////////////////////////
// const
#ifdef WIN32
#pragma warning(disable:4267 4311 4996)

#ifndef _WIN32_WINNT            // Specifies that the minimum required platform is Windows Vista.
#define _WIN32_WINNT 0x0501     // Change this to the appropriate value to target other versions of Windows.
#endif

#include <winsock2.h>
#include <windows.h>

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

#endif // WIN32

// cgc head
#include <CGCBase/comm.h>
#include <ThirdParty/stl/locklist.h>
#include <ThirdParty/stl/lockmap.h>
#include <ThirdParty/Boost/asio/IoService.h>
#include <ThirdParty/Boost/asio/UdpSocket.h>
using namespace cgc;

#include "../CgcRemoteInfo.h"

class CRemoteHandler
{
public:
	virtual unsigned long getCommId(void) const = 0;
	virtual int getProtocol(void) const = 0;
	virtual int getServerPort(void) const = 0;
};
////////////////////////////////////////
// CcgcRemote
class CcgcRemote
	: public cgcRemote
{
private:
	CRemoteHandler * m_handler;
	udp::endpoint m_endpoint;
	unsigned long m_nRemoteId;
	unsigned long m_nIpAddress;
	//UdpEndPoint::pointer m_endpoint;
	udp::socket * m_socket;
	tstring m_sRemoteAddr;			// string of IP address

public:
	// ???
	CcgcRemote(CRemoteHandler * handler, const UdpEndPoint::pointer& endpoint, udp::socket * socket)
		: m_handler(handler), m_endpoint(endpoint->endpoint()), m_nRemoteId(endpoint->getId()),m_nIpAddress(endpoint->getIpAddress())
		, m_socket(socket)
	{
		assert (m_handler != NULL);
		assert (m_socket != NULL);
		unsigned short port = m_endpoint.port();
		std::string address = m_endpoint.address().to_string();

		char bufferTemp[256];
		memset(bufferTemp, 0, 256);
		sprintf(bufferTemp, "%s:%d", address.c_str(), port);
		m_sRemoteAddr = bufferTemp;

//		const boost::asio::detail::sockaddr_in4_type *dt = (const boost::asio::detail::sockaddr_in4_type*)endpoint->endpoint().data();
//#ifdef WIN32
//		printf("%s: %ld==n:%d<->h:%d\n",m_sRemoteAddr.c_str(),dt->sin_addr.S_un.S_addr,dt->sin_port,ntohs(dt->sin_port));
//#else
//		printf("%s: %ld==n:%d<->h:%d\n",m_sRemoteAddr.c_str(),dt->sin_addr.s_addr,dt->sin_port,ntohs(dt->sin_port));
//#endif
		// UDPȡ����remote_endpoint
		//boost::system::error_code ec;
		//udp::endpoint pRemoteEndpoint = socket->remote_endpoint(ec);
		//printf("******** RemoteAddr1 %s: error=%d,%s\n",m_sRemoteAddr.c_str(),ec.value(),ec.message().c_str());
		//printf("******** RemoteAddr2 %s:%d\n",pRemoteEndpoint.address().to_string().c_str(),pRemoteEndpoint.port());
	}
	virtual ~CcgcRemote(void){}

	void SetRemote(const UdpEndPoint::pointer& endpoint)
	{
		m_endpoint = endpoint->endpoint();
		m_nRemoteId = endpoint->getId();
		m_nIpAddress = endpoint->getIpAddress();
	}

private:
	virtual int getProtocol(void) const {return m_handler->getProtocol();}
	virtual int getServerPort(void) const {return m_handler->getServerPort();}
	virtual unsigned long getCommId(void) const {return m_handler->getCommId();}
	virtual unsigned long getRemoteId(void) const {return m_nRemoteId;}
	virtual unsigned long getIpAddress(void) const {return m_nIpAddress;}
	virtual tstring getScheme(void) const {return _T("UDP");}
	virtual const tstring & getRemoteAddr(void) const {return m_sRemoteAddr;}
	//boost::mutex m_sendMutex;
	virtual int sendData(const unsigned char * data, size_t size)
	{
		if (m_socket==NULL || data == NULL || isInvalidate()) return -1;
		try
		{
			//boost::mutex::scoped_lock lock(m_sendMutex);
			boost::system::error_code ignored_error;
			m_socket->send_to(boost::asio::buffer(data, size), m_endpoint, 0, ignored_error);
		}catch (std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			return -2;
		}catch(...)
		{
			return -2;
		}
		return 0;
	}
	virtual void invalidate(bool bClose) {m_socket = 0;}
	virtual bool isInvalidate(void) const {return m_socket == 0;}
};

const int ATTRIBUTE_NAME	= 1;
//const int EVENT_ATTRIBUTE	= 2;
//static unsigned int theCurrentIdEvent	= 0;

#define MAIN_MGR_EVENT_ID	1
#define MIN_EVENT_THREAD	6
#define MAX_EVENT_THREAD	600

//class CIDEvent : public cgcObject
//{
//public:
//	unsigned int m_nCurIDEvent;
//	int m_nCapacity;
//};
cgcAttributes::pointer theAppAttributes;

/////////////////////////////////////////
// CUdpServer
class CUdpServer
	: public UdpSocket_Handler
	, public IoService_Handler
	, public cgcOnTimerHandler
	, public cgcCommunication
	, public CRemoteHandler
	, public boost::enable_shared_from_this<CUdpServer>
{
public:
	typedef boost::shared_ptr<CUdpServer> pointer;
	static CUdpServer::pointer create(int nIndex)
	{
		return CUdpServer::pointer(new CUdpServer(nIndex));
	}

private:
	int m_nIndex;
	int m_commPort;
	//int m_capacity;
	int m_protocol;

	IoService::pointer m_ioservice;
	UdpSocket m_socket;
	CLockMap<unsigned long, cgcRemote::pointer> m_mapCgcRemote;
	int m_nDoCommEventCount;

	CLockListPtr<CCommEventData*> m_listMgr;
	CCommEventDataPool m_pCommEventDataPool;
	int m_nCurrentThread;
	int m_nNullEventDataCount;
	int m_nFindEventDataCount;

public:
	CUdpServer(int nIndex)
		: m_nIndex(nIndex),m_commPort(0), /*m_capacity(1), */m_protocol(0)
		, m_nDoCommEventCount(0)
		, m_pCommEventDataPool(Max_UdpSocket_ReceiveSize,30,50)
		, m_nCurrentThread(0), m_nNullEventDataCount(0), m_nFindEventDataCount(0)

	{
	}
	virtual ~CUdpServer(void)
	{
		finalService();
	}

	virtual tstring serviceName(void) const {return _T("UDPSERVER");}
	virtual bool initService(cgcValueInfo::pointer parameter)
	{
		if (m_commHandler.get() == NULL) return false;
		if (m_bServiceInited) return true;

		if (parameter.get() != NULL && parameter->getType() == cgcValueInfo::TYPE_VECTOR)
		{
			const std::vector<cgcValueInfo::pointer>& lists = parameter->getVector();
			if (lists.size() > 2)
				m_protocol = lists[2]->getInt();
			//if (lists.size() > 1)
			//	m_capacity = lists[1]->getInt();
			if (lists.size() > 0)
				m_commPort = lists[0]->getInt();
			else
				return false;
		}

		// ??
		//m_socket.setMaxBufferSize(10*1024);
		//m_socket.setUnusedSize(0, false);
		if (m_ioservice.get() == NULL)
			m_ioservice = IoService::create();
		m_socket.start(m_ioservice->ioservice(), m_commPort, shared_from_this());
		m_ioservice->start(shared_from_this());

		m_nCurrentThread = MIN_EVENT_THREAD;
		for (int i=1; i<=m_nCurrentThread; i++)
		{
			theApplication->SetTimer((this->m_nIndex*MAX_EVENT_THREAD)+i, 10, shared_from_this());	// 10ms
		}

		//m_capacity = m_capacity < 1 ? 1 : m_capacity;
		//CIDEvent * pIDEvent = new CIDEvent();
		//pIDEvent->m_nCurIDEvent = theCurrentIdEvent+1;
		//pIDEvent->m_nCapacity = m_capacity;
		//theAppAttributes->setAttribute(EVENT_ATTRIBUTE, this, cgcObject::pointer(pIDEvent));

		//for (int i=0; i<m_capacity; i++)
		//{
		//	theApplication->SetTimer(++theCurrentIdEvent, m_capacity, shared_from_this());
		//}

		m_bServiceInited = true;
		CGC_LOG((LOG_INFO, _T("**** [*:%d] Start succeeded ****\n"), m_commPort));
		return true;
	}

	virtual void finalService(void)
	{
		if (!m_bServiceInited) return;

		for (unsigned int i=this->m_nIndex*MAX_EVENT_THREAD+1; i<=this->m_nIndex*MAX_EVENT_THREAD+m_nCurrentThread; i++)
			theApplication->KillTimer(i);
		//cgcObject::pointer eventPointer = theAppAttributes->removeAttribute(EVENT_ATTRIBUTE, this);
		//CIDEvent * pIDEvent = (CIDEvent*)eventPointer.get();
		//if (pIDEvent != NULL)
		//{
		//	for (unsigned int i=pIDEvent->m_nCurIDEvent; i<pIDEvent->m_nCurIDEvent+pIDEvent->m_nCapacity; i++)
		//		theApplication->KillTimer(i);
		//}

		m_socket.stop();
		m_listMgr.clear();
		m_pCommEventDataPool.Clear();
		m_mapCgcRemote.clear();
		m_ioservice.reset();
		m_bServiceInited = false;
		CGC_LOG((LOG_INFO, _T("**** [%s:%d] Stop succeeded ****\n"), serviceName().c_str(), m_commPort));
	}

	unsigned long getId(void) const {return (unsigned long)this;}

private:
	bool eraseIsInvalidated(void)
	{
		BoostReadLock rdlock(m_mapCgcRemote.mutex());
		CLockMap<unsigned long, cgcRemote::pointer>::iterator pIter;
		for (pIter=m_mapCgcRemote.begin(); pIter!=m_mapCgcRemote.end(); pIter++)
		{
			cgcRemote::pointer pCgcRemote = pIter->second;
			if (pCgcRemote->isInvalidate())
			{
				unsigned long nId = pIter->first;
				rdlock.unlock();
				m_mapCgcRemote.remove(nId);
				return true;
			}
		}
		return false;
	}

	// cgcOnTimerHandler

	virtual void OnTimeout(unsigned int nIDEvent, const void * pvParam)
	{
		if (m_commHandler.get() == NULL) return;
		if (nIDEvent==(this->m_nIndex*MAX_EVENT_THREAD)+MAIN_MGR_EVENT_ID)
		{
			if (++m_nDoCommEventCount > 120000)
			{
				if (!eraseIsInvalidated())
				{
					m_nDoCommEventCount = 0;
				}
			}
			const size_t nSize = m_listMgr.size();
			if (nSize>(m_nCurrentThread+20))
			{
				m_nNullEventDataCount = 0;
				m_nFindEventDataCount++;
				if (m_nCurrentThread<MAX_EVENT_THREAD && (nSize>(MAX_EVENT_THREAD*2) || (nSize>(m_nCurrentThread*2)&&m_nFindEventDataCount>20) || m_nFindEventDataCount>100))	// 100*10ms=1S
				{
					m_nFindEventDataCount = 0;
					const unsigned int nNewTimerId = (this->m_nIndex*MAX_EVENT_THREAD)+(++m_nCurrentThread);
					printf("**** UDPServer:NewTimerId=%d size=%d ****\n",nNewTimerId,nSize);
					theApplication->SetTimer(nNewTimerId, 10, shared_from_this());	// 10ms
				}
			}else
			{
				m_nFindEventDataCount = 0;
				m_nNullEventDataCount++;
				if (m_nCurrentThread>MIN_EVENT_THREAD && ((nSize<(m_nCurrentThread/2)&&m_nNullEventDataCount>200) || m_nNullEventDataCount>300))	// 300*10ms=3S
				{
					m_nNullEventDataCount = 0;
					const unsigned int nKillTimerId = (this->m_nIndex*MAX_EVENT_THREAD)+m_nCurrentThread;
					printf("**** UDPServer:KillTimerId=%d ****\n",nKillTimerId);
					theApplication->KillTimer(nKillTimerId);
					m_nCurrentThread--;
				}
			}
			return;
		}
		CCommEventData * pCommEventData = m_listMgr.front();
		if (pCommEventData == NULL)
		{
			return;
		}

		switch (pCommEventData->getCommEventType())
		{
		case CCommEventData::CET_Recv:
			m_commHandler->onRecvData(pCommEventData->getRemote(), pCommEventData->getRecvData(), pCommEventData->getRecvSize());
			break;
		case CCommEventData::CET_Exception:
			{
				CGC_LOG((LOG_ERROR, _T("**** [%s:%d] CET_Exception ****\n"), serviceName().c_str(), m_commPort));
				if (!m_listMgr.empty())
				{
					m_listMgr.clear();

					//AUTO_WLOCK(m_listMgr);
					//CLockListPtr<CCommEventData*>::iterator pIter = m_listMgr.begin();
					//for (; pIter!=m_listMgr.end(); pIter++)
					//{
					//	CCommEventData * pCloseCommEventData = m_listMgr.front();
					//	m_commHandler->onRemoteClose(pCloseCommEventData->getRemoteId(),pCloseCommEventData->GetErrorCode());
					//}
					//m_listMgr.clear(false,true);
				}
				m_mapCgcRemote.clear();
				m_socket.stop();
				m_ioservice.reset();
				m_ioservice = IoService::create();
				m_socket.start(m_ioservice->ioservice(), m_commPort, shared_from_this());
				m_ioservice->start(shared_from_this());
				CGC_LOG((LOG_INFO, _T("**** [%s:%d] Restart OK ****\n"), serviceName().c_str(), m_commPort));
			}break;
		default:
			break;
		}
		m_pCommEventDataPool.Set(pCommEventData);
		//delete pCommEventData;
	}

	// CRemoteHandler
	virtual unsigned long getCommId(void) const {return getId();}
	virtual int getProtocol(void) const {return m_protocol;}
	virtual int getServerPort(void) const {return m_commPort;}

	// IoService_Handler
	virtual void OnIoServiceException(void)
	{
		CCommEventData * pEventData = new CCommEventData(CCommEventData::CET_Exception);
		m_listMgr.add(pEventData);
	}

	// UdpSocket_Handler
	virtual void OnReceiveData(const UdpSocket & socket2, const UdpEndPoint::pointer& endpoint)
	{
		if (endpoint->size() == 0) return;
		//printf("*** OnReceiveData size=%d\n",endpoint->size());

		if (m_commHandler.get() != NULL)
		{
			u_long remoteAddrHash = endpoint->getId();	

			cgcRemote::pointer pCgcRemote;
			if (!m_mapCgcRemote.find(remoteAddrHash, pCgcRemote))
			{
				pCgcRemote = cgcRemote::pointer(new CcgcRemote((CRemoteHandler*)this, endpoint, m_socket.socket()));
				m_mapCgcRemote.insert(remoteAddrHash, pCgcRemote);
			}else if (pCgcRemote->isInvalidate())
			{
				m_mapCgcRemote.remove(remoteAddrHash);
				pCgcRemote = cgcRemote::pointer(new CcgcRemote((CRemoteHandler*)this, endpoint, m_socket.socket()));
				m_mapCgcRemote.insert(remoteAddrHash, pCgcRemote);
			}else
			{
				((CcgcRemote*)pCgcRemote.get())->SetRemote(endpoint);
			}

			//CCommEventData * pEventData = new CCommEventData(CCommEventData::CET_Recv);
			CCommEventData * pEventData = m_pCommEventDataPool.Get();
			pEventData->setCommEventType(CCommEventData::CET_Recv);
			pEventData->setRemote(pCgcRemote);
			pEventData->setRecvData(endpoint->buffer(), endpoint->size());
			m_listMgr.add(pEventData);
		}
	}
};

extern "C" bool CGC_API CGC_Module_Init(void)
{
#ifdef WIN32
	WSADATA wsaData;
	int err = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
	if ( err != 0 ) {
		theApplication->log(LOG_ERROR, _T("WSAStartup' error!\n"));
		return false;
	}
#endif // WIN32

	theAppAttributes = theApplication->getAttributes(true);
	assert (theAppAttributes.get() != NULL);
	return true;
}

extern "C" void CGC_API CGC_Module_Free(void)
{
	VoidObjectMapPointer mapLogServices = theAppAttributes->getVoidAttributes(ATTRIBUTE_NAME, false);
	if (mapLogServices.get() != NULL)
	{
		CObjectMap<void*>::iterator iter;
		for (iter=mapLogServices->begin(); iter!=mapLogServices->end(); iter++)
		{
			CUdpServer::pointer commServer = CGC_OBJECT_CAST<CUdpServer>(iter->second);
			if (commServer.get() != NULL)
			{
				commServer->finalService();
			}
		}
	}
	
	theAppAttributes->clearAllAtrributes();
	theAppAttributes.reset();
	theApplication->KillAllTimer();
#ifdef WIN32
	WSACleanup();
#endif // WIN32
}

int theServiceIndex = 0;
extern "C" void CGC_API CGC_GetService(cgcServiceInterface::pointer & outService, const cgcValueInfo::pointer& parameter)
{
	CUdpServer::pointer commServer = CUdpServer::create(theServiceIndex++);
	outService = commServer;

	theAppAttributes->setAttribute(ATTRIBUTE_NAME, outService.get(), commServer);
}

extern "C" void CGC_API CGC_ResetService(const cgcServiceInterface::pointer & inService)
{
	if (inService.get() == NULL) return;

	theAppAttributes->removeAttribute(ATTRIBUTE_NAME, inService.get());
	inService->finalService();
}
