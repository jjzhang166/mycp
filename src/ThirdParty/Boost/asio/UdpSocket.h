// UdpSocket.h file here
#ifndef __UdpSocket_h__
#define __UdpSocket_h__

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "../../stl/locklist.h"
using boost::asio::ip::udp;
#include "UdpEndPoint.h"

//////////////////////////////////////////////
// UdpSocket_Handler
class UdpSocket;
class UdpSocket_Handler
{
public:
	typedef boost::shared_ptr<UdpSocket_Handler> pointer;
	virtual void OnReceiveData(const UdpSocket& UdpSocket, const UdpEndPoint::pointer& endpoint) = 0;
};
const UdpSocket_Handler::pointer NullUdpSocketHandler;

///////////////////////////////////////////////
// UdpSocket class
class UdpSocket
{
public:
	typedef boost::shared_ptr<UdpSocket> pointer;
	static UdpSocket::pointer create(size_t nBufferSize=8*1024) {return UdpSocket::pointer(new UdpSocket(nBufferSize));}

	void setMaxBufferSize(size_t v = 8*1024)
	{
		m_maxbuffersize = v;
		UdpEndPoint::pointer pPoolData;
		while (m_pool.front(pPoolData))
		{
			if (pPoolData->bufferSize()>=(m_maxbuffersize+1))
			{
				m_pool.add(pPoolData);
				break;
			}
		}
	}
	void setPoolSize(size_t nInitPoolSize = 20, size_t nMaxPoolSize = 30)
	{
		m_nInitPoolSize = nInitPoolSize;
		m_nMaxPoolSize = nMaxPoolSize;
		while (m_pool.size()<m_nInitPoolSize)
			m_pool.add(UdpEndPoint::create(m_maxbuffersize));
	}

	// udpPort == 0; ¶¯Ì¬
	// nThtreadStackSize 100=100KB
	void start(boost::asio::io_service & ioservice, unsigned short udpPort, const UdpSocket_Handler::pointer& handler, int nThreadStackSize=100)
	{
		m_handler = handler;

		if (m_socket == NULL)
		{
			const udp::endpoint pEndPoint(udp::endpoint(udp::v4(), udpPort));
			m_socket = new udp::socket(ioservice, pEndPoint);
			//m_socket->bind(pEndPoint);	// exception
			m_socket->set_option(boost::asio::socket_base::reuse_address(true));
			m_socket->set_option(boost::asio::socket_base::send_buffer_size(64*1024));		// 8192
			m_socket->set_option(boost::asio::socket_base::receive_buffer_size(64*1024));
		}

		if (m_proc_data == 0)
		{
			boost::thread_attributes attrs;
			attrs.set_stack_size(1024*nThreadStackSize);	// 100K
			m_proc_data = new boost::thread(attrs,boost::bind(&UdpSocket::do_proc_data, this));
			//m_proc_data = new boost::thread(boost::bind(&UdpSocket::do_proc_data, this));
		}
		start_receive();
	}
	void stop(void)
	{
		udp::socket * pSocketTemp = m_socket;
		m_socket = NULL;
		if (pSocketTemp)
			delete pSocketTemp;

		if (m_proc_data)
		{
			m_proc_data->join();
			delete m_proc_data;
			m_proc_data = 0;
		}

		m_endpoints.clear();
		m_pool.clear();
		m_handler.reset();
	}
	bool is_start(void) const {return m_socket != NULL;}
	udp::socket * socket(void) {return m_socket;}
	void write(const unsigned char * data, size_t size, const udp::endpoint & endpoint)
	{
		boost::system::error_code ignored_error;
		if (m_socket)
		{
			m_socket->send_to(boost::asio::buffer(data, size), endpoint, 0, ignored_error);
		}
	}
	void send(const unsigned char * data, size_t size)
	{
		boost::system::error_code ignored_error;
		if (m_socket)
		{
			m_socket->send(boost::asio::buffer(data, size), 0, ignored_error);
		}
	}

	void proc_Data(void)
	{
		unsigned short nIdleCount = 0;
		while (m_socket != 0)
		{
			UdpEndPoint::pointer endpoint;
			if (!m_endpoints.front(endpoint))
			{
				if ((nIdleCount++)>500*5) // 5S
				{
					nIdleCount = 0;
					if (m_pool.size()>m_nInitPoolSize)
						m_pool.front(endpoint);
				}
#ifdef WIN32
				Sleep(2);
#else
				usleep(2000);
#endif
				continue;
			}
			nIdleCount = 0;

			if (m_handler.get() != NULL)
			{
				try
				{
					m_handler->OnReceiveData(*this, endpoint);
				}catch(std::exception&)
				{
				}catch(...)
				{}
			}

			if (m_pool.size()<m_nMaxPoolSize)
			{
				if (endpoint->init(m_maxbuffersize))
					m_pool.add(endpoint);
			}
		}
	}

private:
	void start_receive(void)
	{
		if (m_socket)
		{
			//UdpEndPoint::pointer new_endpoint = UdpEndPoint::create(m_maxbuffersize);
			UdpEndPoint::pointer new_endpoint;
			if (!m_pool.front(new_endpoint))
				new_endpoint = UdpEndPoint::create(m_maxbuffersize);
			m_socket->async_receive_from(boost::asio::buffer(const_cast<unsigned char*>(new_endpoint->buffer()), m_maxbuffersize),
				new_endpoint->endpoint(),
				boost::bind(&UdpSocket::receive_handler, this, new_endpoint,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}
	}

	void receive_handler(const UdpEndPoint::pointer& endpoint, const boost::system::error_code& error, std::size_t size)
	{
		endpoint->init2();
		endpoint->size(size);
		m_endpoints.add(endpoint);
		//if (m_handler)
		//	m_handler->OnReceiveData(*this, endpoint, size);
		try
		{
			start_receive();
		}catch(std::exception&)
		{
		}catch(...)
		{}
	}

	static void do_proc_data(UdpSocket * owner)
	{
		BOOST_ASSERT (owner != 0);
		owner->proc_Data();
	}
public:
	UdpSocket(size_t nBufferSize=Max_UdpSocket_ReceiveSize)
		: m_socket(NULL)
		, m_proc_data(0),m_maxbuffersize(nBufferSize),m_nInitPoolSize(20), m_nMaxPoolSize(50)
	{
	}
	virtual ~UdpSocket(void)
	{
		stop();
	}
private:
	UdpSocket_Handler::pointer m_handler;
	udp::socket * m_socket;
	udp::endpoint m_endpointlocal;
	boost::thread * m_proc_data;
	CLockList<UdpEndPoint::pointer> m_endpoints;
	size_t m_maxbuffersize;
	size_t m_nInitPoolSize;
	size_t m_nMaxPoolSize;
	CLockList<UdpEndPoint::pointer> m_pool;
};

#endif // __UdpSocket_h__
