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

#include "CgcUdpClient.h"

namespace cgc
{
CgcUdpClient::CgcUdpClient(void)
: CgcBaseClient(_T("UDP"))

{
}

CgcUdpClient::~CgcUdpClient(void)
{
	StopClient();
}

int CgcUdpClient::startClient(const tstring & sCgcServerAddr, unsigned int bindPort)
{
	if (m_udpClient.get() != 0) return 0;

	if (!setRemoteAddr(sCgcServerAddr))  return -1;
	try
	{
		if (m_ipService.get() == 0)
			m_ipService = IoService::create();

		if (m_udpClient.get()==NULL)
			m_udpClient = UdpSocket::create();

		CgcUdpClient::pointer clientHandler = boost::static_pointer_cast<CgcUdpClient, CgcBaseClient>(boost::enable_shared_from_this<CgcBaseClient>::shared_from_this());

		m_udpClient->start(m_ipService->ioservice(), bindPort, clientHandler, 1024);
		m_udpClient->socket()->connect(m_endpointRemote);
		m_ipService->start();
		// **测试，不能删除；
#ifdef WIN32
		Sleep(50);
#else
		usleep(50000);
#endif
		m_endpointLocal = m_udpClient->socket()->local_endpoint();
		m_ipLocal = CCgcAddress(m_endpointLocal.address().to_string(), m_endpointLocal.port(), CCgcAddress::ST_UDP);
	}catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return -2;
	}
	return 0;
}

void CgcUdpClient::stopClient(void)
{
	if (m_udpClient.get() != 0)
	{
		m_udpClient->stop();
	}
	if (m_ipService.get() != 0)
	{
		m_ipService->stop();
	}
	m_udpClient.reset();
	m_ipService.reset();
}

size_t CgcUdpClient::sendData(const unsigned char * data, size_t size)
{
	BOOST_ASSERT(m_udpClient.get() != 0);
	if (data == NULL || isInvalidate()) return 0;

	m_tSendRecv = time(0);
	m_udpClient->send(data, size);
	//m_udpClient->write(data, size, m_endpointRemote);	// **不需要用这个，因为下面已经用了 m_udpClient->socket()->connect(m_endpointRemote);
	return 0;
}

bool CgcUdpClient::isInvalidate(void) const
{
	return m_udpClient.get() == 0 || !m_udpClient->is_start();
}

bool CgcUdpClient::setRemoteAddr(const tstring & sRemoteAddr)
{
	if (sRemoteAddr.empty()) return false;
	if (m_ipRemote.address()==sRemoteAddr)
		return true;
	std::vector<std::string> pList;
	if (CgcBaseClient::ParseString(sRemoteAddr.c_str(),":",pList)==2)
	{
		std::string sIp;
		for (int i=0;i<20;i++)
		{
			sIp = CgcBaseClient::GetHostIp(pList[0].c_str(),"");
			if (!sIp.empty())
				break;
#ifdef WIN32
			Sleep(100);
#else
			usleep(100000);
#endif
		}
		if (sIp.empty())
		{
			// *
			return false;
			sIp = pList[0];
		}
		unsigned short nPort = atoi(pList[1].c_str());
		if (m_ipRemote.getport()!=nPort || m_ipRemote.getip()!=sIp)
		{
			m_ipRemote.address(sIp,nPort);
			m_endpointRemote.address(boost::asio::ip::address_v4::from_string(sIp.c_str()));
			m_endpointRemote.port(nPort);
			//m_endpointRemote = udp::endpoint(boost::asio::ip::address_v4::from_string(sIp.c_str()), nPort);
		}
		if (m_udpClient.get()!=NULL && m_udpClient->socket()!=NULL)
		{
			boost::system::error_code ec;
			m_udpClient->socket()->connect(m_endpointRemote,ec);
		}
		return true;
	}
	return false;
}
bool CgcUdpClient::doSetConfig(int nConfig, unsigned int nInValue)
{
	if (!CgcBaseClient::doSetConfig(nConfig,nInValue))
		return false;
	switch (nConfig)
	{
	case SOTP_CLIENT_CONFIG_MAX_RECEIVE_BUFFER_SIZE:
		{
			if (m_udpClient.get()==NULL)
			{
				m_udpClient = UdpSocket::create(nInValue);
			}else
			{
				m_udpClient->setMaxBufferSize(nInValue);
			}
		}
		break;
	default:
		break;
	}
	return true;
}
void CgcUdpClient::doGetConfig(int nConfig, unsigned int* nOutValue) const
{
	CgcBaseClient::doGetConfig(nConfig,nOutValue);
	switch (nConfig)
	{
	case SOTP_CLIENT_CONFIG_MAX_RECEIVE_BUFFER_SIZE:
		{
			*nOutValue = 0;
		}break;
	case SOTP_CLIENT_CONFIG_USES_SSL:
		{

		}break;
	default:
		break;
	}
}

void CgcUdpClient::OnReceiveData(const UdpSocket & UdpSocket, const UdpEndPoint::pointer& endpoint)
{
	if (endpoint->size() <= 0) return;

	m_tSendRecv = time(0);
	//m_endpointRemote = endpoint->endpoint();
	//if (m_ipRemote.getport()!=m_endpointRemote.port() || m_ipRemote.getip()!=m_endpointRemote.address().to_string())
	//{
	//	m_ipRemote = CCgcAddress(m_endpointRemote.address().to_string(),m_endpointRemote.port(), CCgcAddress::ST_UDP);
	//}
	this->parseData(endpoint->buffer(), endpoint->size(),endpoint->getId());
	//this->parseData(CCgcData::create(endpoint->buffer(), endpoint->size()),endpoint->getId());
}

} // namespace cgc
