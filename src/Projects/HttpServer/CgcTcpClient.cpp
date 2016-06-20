#ifdef WIN32
#pragma warning(disable:4267 4819 4996)
#ifndef _WIN32_WINNT            // Specifies that the minimum required platform is Windows Vista.
#define _WIN32_WINNT 0x0501     // Change this to the appropriate value to target other versions of Windows.
#endif
//#include <winsock2.h>
//#include <windows.h>
#endif // WIN32

#include "CgcTcpClient.h"
#include "cgcaddress.h"

namespace mycp {
namespace httpserver {

	CgcTcpClient::CgcTcpClient(TcpClient_Callback* pCallback, int nUserData)
		: m_bExitStopIoService(true)
		, m_connectReturned(false)
		, m_bDisconnect(true)
		, m_bException(false)
		, m_pCallback(pCallback)
		, m_nUserData(nUserData)
		//, m_sReceiveData(_T(""))
	{
	}
	CgcTcpClient::CgcTcpClient(void)
		: m_bExitStopIoService(true)
		, m_connectReturned(false)
		, m_bDisconnect(true)
		, m_bException(false)
		, m_pCallback(NULL)
		//, m_sReceiveData(_T(""))
		, m_nUserData(0)
	{
	}
	CgcTcpClient::~CgcTcpClient(void)
	{
		stopClient();
	}

	std::string CgcTcpClient::GetHostIp(const char * lpszHostName,const char* lpszDefault)
	{
		try
		{
			struct hostent *host_entry;
			//struct sockaddr_in addr;
			/* 即要解析的域名或主机名 */
			host_entry=gethostbyname(lpszHostName);
			//printf("%s\n", dn_or_ip);
			if(host_entry!=0)
			{
				char lpszIpAddress[50];
				memset(lpszIpAddress, 0, sizeof(lpszIpAddress));
				//printf("解析IP地址: ");
				sprintf(lpszIpAddress, "%d.%d.%d.%d",
					(host_entry->h_addr_list[0][0]&0x00ff),
					(host_entry->h_addr_list[0][1]&0x00ff),
					(host_entry->h_addr_list[0][2]&0x00ff),
					(host_entry->h_addr_list[0][3]&0x00ff));
				return lpszIpAddress;
			}
		}catch(std::exception&)
		{
		}catch(...)
		{}
		return lpszDefault;
	}

#ifdef USES_OPENSSL
	int CgcTcpClient::startClient(const cgc::tstring & sCgcServerAddr, unsigned int bindPort,boost::asio::ssl::context* ctx)
#else
	int CgcTcpClient::startClient(const cgc::tstring & sCgcServerAddr, unsigned int bindPort)
#endif
	{
		if (m_tcpClient.get() != 0) return 0;

		mycp::httpserver::CCgcAddress cgcAddress = mycp::httpserver::CCgcAddress(sCgcServerAddr, mycp::httpserver::CCgcAddress::ST_TCP);
		const unsigned short nPort = (unsigned short)cgcAddress.getport();
		if (nPort==0) return -1;

		const cgc::tstring sInIp = cgcAddress.getip();
		std::string sIp;
		for (int i=0;i<5;i++)
		{
			sIp = mycp::httpserver::CgcTcpClient::GetHostIp(sInIp.c_str(),"");
			if (!sIp.empty())
				break;
#ifdef WIN32
			Sleep(100);
#else
			usleep(100000);
#endif
		}
		if (sIp.empty())
			sIp = sInIp.c_str();

		if (m_ipService.get() == 0)
			m_ipService = mycp::asio::IoService::create();

		TcpClient_Handler::pointer clientHandler = boost::enable_shared_from_this<CgcTcpClient>::shared_from_this();
		//CgcTcpClient::pointer clientHandler = boost::static_pointer_cast<CgcTcpClient, CgcBaseClient>(boost::enable_shared_from_this<CgcBaseClient>::shared_from_this());

		m_tcpClient = TcpClient::create(clientHandler);

		m_connectReturned = false;
		// ?? bindPort
		boost::system::error_code ec;
		tcp::endpoint endpoint(boost::asio::ip::address_v4::from_string(sIp.c_str(),ec), nPort);
#ifdef USES_OPENSSL
		m_tcpClient->connect(m_ipService->ioservice(), endpoint,ctx);
#else
		m_tcpClient->connect(m_ipService->ioservice(), endpoint);
#endif
		m_ipService->start(shared_from_this());
		for (int i=0; i<500; i++)	// max 3S
		{
			if (m_connectReturned) break;
#ifdef WIN32
			Sleep(10);
#else
			usleep(10000);
#endif
		}
		//if (!m_bDisconnect)
		//{
		//	m_tcpClient->socket()->get_socket()->set_option(boost::asio::socket_base::send_buffer_size(64*1024),ec);
		//	m_tcpClient->socket()->get_socket()->set_option(boost::asio::socket_base::receive_buffer_size(6*1024),ec);
		//}
		return 0;
	}

	void CgcTcpClient::stopClient(void)
	{
		m_pCallback = NULL;
		if (m_tcpClient.get() != 0)
		{
			//m_tcpClient->resetHandler();
			m_tcpClient->disconnect();
		}
		if (m_bExitStopIoService && m_ipService.get() != 0)
		{
			m_ipService->stop();
		}
		m_tcpClient.reset();
		m_ipService.reset();
	}

	size_t CgcTcpClient::sendData(const unsigned char * data, size_t size)
	{
		BOOST_ASSERT(m_tcpClient.get() != 0);

		//printf("******** CgcTcpClient::sendData1\n");

		if (IsDisconnection() || IsException() || data == 0 || isInvalidate()) return 0;

		//printf("******** CgcTcpClient::sendData2\n");
		//const size_t s = m_tcpClient->write(data, size);
		//m_tcpClient->async_read_some();
		//return s;
		return m_tcpClient->write(data, size);
		//return m_tcpClient->write(data, size);
	}

	bool CgcTcpClient::isInvalidate(void) const
	{
		return m_tcpClient.get() == 0 || !m_tcpClient->is_open();
	}

	void CgcTcpClient::OnConnected(const TcpClientPointer& tcpClient)
	{
		//BOOST_ASSERT (tcpClient.get() != 0);
		//printf("******** CgcTcpClient::OnConnected\n");

		m_connectReturned = true;
		m_bDisconnect = false;
		if (m_pCallback != NULL)
			m_pCallback->OnConnected(m_nUserData);

		//if (tcpClient->is_open())
		//{
		//	tcp::endpoint local_endpoint = tcpClient->socket()->local_endpoint();
		//	tcp::endpoint remote_endpoint = tcpClient->socket()->remote_endpoint();
		//	m_ipLocal = CCgcAddress(local_endpoint.address().to_string(), local_endpoint.port(), CCgcAddress::ST_TCP);
		//	m_ipRemote = CCgcAddress(remote_endpoint.address().to_string(), remote_endpoint.port(), CCgcAddress::ST_TCP);
		//}
	}

	void CgcTcpClient::OnConnectError(const TcpClientPointer& tcpClient, const boost::system::error_code & error)
	{
		//printf("******** CgcTcpClient::OnConnectError %d\n", error.value());
		m_connectReturned = true;
		m_bDisconnect=true;
	}

	void CgcTcpClient::OnReceiveData(const TcpClientPointer& tcpClient, const mycp::asio::ReceiveBuffer::pointer& data)
	{
		BOOST_ASSERT (tcpClient.get() != 0);
		BOOST_ASSERT (data.get() != 0);
		//printf("******** CgcTcpClient::OnReceiveData\n");

		if (data->size() <= 0) return;
		if (m_pCallback != NULL)
			m_pCallback->OnReceiveData(data, m_nUserData);
		//unsigned char lpszBuffer[1024];
		//memcpy(lpszBuffer,data->data(),data->size());
		//m_tSendRecv = time(0);
		//this->parseData(CCgcData::create(data->data(), data->size()));
		//m_sReceiveData.append(tstring((const char*)data->data(), data->size()));
	}
	void CgcTcpClient::OnDisconnect(const TcpClientPointer& tcpClient, const boost::system::error_code & error)
	{
		//printf("******** CgcTcpClient::OnDisconnect %d\n",error.value());
		m_connectReturned = true;m_bDisconnect=true;
		if (m_pCallback != NULL)
			m_pCallback->OnDisconnect(m_nUserData);
	}

} // namespace httpserver
} // namespace mycp
