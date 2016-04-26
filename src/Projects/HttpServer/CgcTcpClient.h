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

// CgcTcpClient.h file here
#ifndef __CgcTcpClient_h__
#define __CgcTcpClient_h__

//
// include
//#include "CgcBaseClient.h"
#include <CGCBase/cgcSmartString.h>
#include <ThirdParty/Boost/asio/IoService.h>
#include <ThirdParty/Boost/asio/boost_socket.h>
#include <ThirdParty/Boost/asio/TcpClient.h>

namespace cgc
{
class TcpClient_Callback
{
public:
	typedef boost::shared_ptr<TcpClient_Callback> pointer;
	virtual void OnReceiveData(const ReceiveBuffer::pointer& data, int nUserData) = 0;
	virtual void OnConnected(int nUserData){}
	virtual void OnDisconnect(int nUserData){}
};

class CgcTcpClient
	: public TcpClient_Handler
	, public IoService_Handler
	, public boost::enable_shared_from_this<CgcTcpClient>
{
public:
	typedef boost::shared_ptr<CgcTcpClient> pointer;

	static CgcTcpClient::pointer create(TcpClient_Callback* pCallback, int nUserData)
	{
		return CgcTcpClient::pointer(new CgcTcpClient(pCallback, nUserData));
	}

	CgcTcpClient(void);
	CgcTcpClient(TcpClient_Callback* pCallback, int nUserData);
	virtual ~CgcTcpClient(void);

	static std::string GetHostIp(const char * lpszHostName,const char* lpszDefault);

#ifdef USES_OPENSSL
	virtual int startClient(const tstring & sCgcServerAddr, unsigned int bindPort,boost::asio::ssl::context* ctx=NULL);
#else
	virtual int startClient(const tstring & sCgcServerAddr, unsigned int bindPort);
#endif
	virtual void stopClient(void);
	virtual size_t sendData(const unsigned char * data, size_t size);
	//const tstring & GetReceiveData(void) const {return m_sReceiveData;}
	//void ClearData(void) {m_sReceiveData.clear();}
	bool IsDisconnection(void) const {return m_bDisconnect;}
	bool IsException(void) const {return m_bException;}
	void SetUserData(int nUserData) {m_nUserData = nUserData;}

private:
	virtual bool isInvalidate(void) const;

	// IoService_Handler
	virtual void OnIoServiceException(void){m_bException = true;}
	///////////////////////////////////////////////
	// for TcpClient_Handler
	virtual void OnConnected(const TcpClientPointer& tcpClient);
	virtual void OnConnectError(const TcpClientPointer& tcpClient, const boost::system::error_code & error);
	virtual void OnReceiveData(const TcpClientPointer& tcpClient, const ReceiveBuffer::pointer& data);
	virtual void OnDisconnect(const TcpClientPointer& tcpClient, const boost::system::error_code & error);

private:
	IoService::pointer m_ipService;
	TcpClient::pointer m_tcpClient;
	bool m_connectReturned;
	bool m_bDisconnect;
	bool m_bException;

	TcpClient_Callback* m_pCallback;
	//tstring m_sReceiveData;
	int m_nUserData;
};

} // namespace cgc
#endif // __CgcTcpClient_h__