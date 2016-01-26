// TcpClient.h file here
#ifndef __TcpClient_h__
#define __TcpClient_h__

#include <boost/asio.hpp>
#ifdef USES_OPENSSL
#include "boost_socket.h"
#endif
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "../../stl/locklist.h"
using boost::asio::ip::tcp;
#include "ReceiveBuffer.h"

//////////////////////////////////////////////
// TcpClient_Handler
class TcpClient;
typedef boost::shared_ptr<TcpClient> TcpClientPointer;
class TcpClient_Handler
{
public:
	typedef boost::shared_ptr<TcpClient_Handler> pointer;
	
	virtual void OnConnected(const TcpClientPointer& tcpClient) = 0;
	virtual void OnConnectError(const TcpClientPointer& tcpClient, const boost::system::error_code & error) = 0;
	virtual void OnReceiveData(const TcpClientPointer& tcpClient, const ReceiveBuffer::pointer& data) = 0;
	virtual void OnDisconnect(const TcpClientPointer & tcpClient,const boost::system::error_code & error) = 0;
};

const TcpClient_Handler::pointer NullTcpClientHandler;

///////////////////////////////////////////////
// TcpClient class
class TcpClient
	: public boost::enable_shared_from_this<TcpClient>
{
public:
	typedef boost::shared_ptr<TcpClient> pointer;
	static TcpClient::pointer create(const TcpClient_Handler::pointer& handler)
	{
		return TcpClient::pointer(new TcpClient(handler));
	}

	void setUnusedSize(size_t v = 10) {m_unusedsize = v;}
	void setMaxBufferSize(size_t v = Max_ReceiveBuffer_ReceiveSize) {m_maxbuffersize = v;}

#ifdef USES_OPENSSL
	void connect(boost::asio::io_service& io_service, tcp::endpoint& endpoint,boost::asio::ssl::context* ctx=NULL, bool bReceiveData=true)
#else
	void connect(boost::asio::io_service& io_service, tcp::endpoint& endpoint, bool bReceiveData=true)
#endif
	{
		if (m_socket == NULL)
		{
#ifdef USES_OPENSSL
			if (ctx)
			{
				//ctx->set_password_callback(boost::bind(&TcpClient::getSSLPassword, this));
				m_socket = new boost_ssl_socket<tcp::socket>(io_service,*ctx);
				m_socket->get_ssl_socket()->set_verify_mode(boost::asio::ssl::verify_peer);  
				m_socket->get_ssl_socket()->set_verify_callback(boost::bind(&TcpClient::verify_certificate, this, _1, _2)); 
				//ctx->set_password_callback(boost::bind(&TcpClient::get_password, this));
				//m_socket->get_ssl_socket()->set_option(boost::asio::socket_base::send_buffer_size(64*1024));
				//m_socket->get_ssl_socket()->set_option(boost::asio::socket_base::receive_buffer_size(64*1024));
			}else
			{
				m_socket = new boost_socket<tcp::socket>(io_service);
				boost::system::error_code ec;
				m_socket->get_socket()->set_option(boost::asio::socket_base::send_buffer_size(64*1024),ec);
				m_socket->get_socket()->set_option(boost::asio::socket_base::receive_buffer_size(64*1024),ec);
			}
#else
			m_socket = new tcp::socket(io_service);
			boost::system::error_code ec;
			m_socket->set_option(boost::asio::socket_base::send_buffer_size(64*1024),ec);
			m_socket->set_option(boost::asio::socket_base::receive_buffer_size(64*1024),ec);
#endif
		}
		if (m_proc_data == 0 && bReceiveData)
			m_proc_data = new boost::thread(boost::bind(&TcpClient::do_proc_data, this));

#ifdef USES_OPENSSL
		m_socket->lowest_layer().async_connect(endpoint,
			//m_socket.async_connect(endpoint,
			boost::bind(&TcpClient::connect_handler, this,
			boost::asio::placeholders::error)
			);

		//if (m_socket->is_ssl())
		//{
		//	boost::asio::ssl::stream<tcp::socket> * p = m_socket->get_ssl_socket();
		//	p->async_handshake(boost::asio::ssl::stream_base::client,boost::bind(&TcpClient::handle_handshake,
		//		this,shared_from_this(),boost::asio::placeholders::error));
		//}else
		//{
		//	m_socket->get_socket()->async_connect(endpoint,
		//		//m_socket.async_connect(endpoint,
		//		boost::bind(&TcpClient::connect_handler, this,
		//		boost::asio::placeholders::error)
		//		);
		//}
#else
		m_socket->async_connect(endpoint,
			//m_socket.async_connect(endpoint,
			boost::bind(&TcpClient::connect_handler, this,
			boost::asio::placeholders::error)
			);
#endif
	}
	void disconnect(void)
	{
		if (m_socket)
		{
#ifdef USES_OPENSSL
			boost_socket_base<tcp::socket>* socktetemp = m_socket;
#else
			tcp::socket * socktetemp = m_socket;
#endif
			m_socket = NULL;
			socktetemp->close();
			delete socktetemp;
		}

		if (m_proc_data)
		{
			m_proc_data->join();
			delete m_proc_data;
			m_proc_data = 0;
		}

		m_datas.clear();
		m_unused.clear();
		//m_socket.close();
	}
#ifdef USES_OPENSSL
	//void setSSLPassword(const tstring& sPassword) {m_sSSLPassword = sPassword;}
	//std::string getSSLPassword(void) const {return m_sSSLPassword;}

	bool verify_certificate(bool preverified, boost::asio::ssl::verify_context& ctx)  
	{  
		// The verify callback can be used to check whether the certificate that is  
		// being presented is valid for the peer. For example, RFC 2818 describes  
		// the steps involved in doing this for HTTPS. Consult the OpenSSL  
		// documentation for more details. Note that the callback is called once  
		// for each certificate in the certificate chain, starting from the root  
		// certificate authority.  

		// In this example we will simply print the certificate's subject name.  
		try
		{
			char subject_name[512];  
			X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
			if (cert!=NULL)
			{
				X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 512);
				//std::cout << "Verifying " << subject_name << "\n";  
			}
		}catch(const std::exception&)
		{}
		catch(const boost::exception&)
		{}
		catch(...)
		{}
		return true;
		//return preverified;  
	} 
	void handle_handshake(const TcpClient::pointer& pTcpClient,const boost::system::error_code& error)
	{
		if(!error)
		{
			if (m_handler.get() != NULL)
			{
				//m_handler->OnConnected(*this);
				m_handler->OnConnected(shared_from_this());
			}
			async_read_some();
		}else
		{
			std::string sError = error.message();
			m_socket->close();
			if (m_handler.get() != NULL)
			{
				//m_handler->OnConnectError(*this, error);
				m_handler->OnConnectError(shared_from_this(), error);
			}
		}
	}
#endif
	bool is_open(void) const
	{
		return m_socket != NULL && m_socket->is_open();
		//return m_socket.is_open();
	}
	size_t write(const unsigned char * data, size_t size)
	{
		if (m_socket == 0) return 0;
		boost::system::error_code ec;
#ifdef USES_OPENSSL
		return boost::asio::write(*m_socket, boost::asio::buffer(data,size), boost::asio::transfer_all(), ec);
#else
		return boost::asio::write(*m_socket, boost::asio::buffer(data,size), boost::asio::transfer_all(), ec);
		//return boost::asio::write(*m_socket, boost::asio::buffer(data, size),ec);
#endif
	}

#ifdef USES_OPENSSL
	boost_socket_base<tcp::socket>* socket(void) {return m_socket;}
	tcp::socket::lowest_layer_type& lowest_layer(void) {return m_socket->lowest_layer();}
#else
	tcp::socket * socket(void) const {return m_socket;}
#endif

	void proc_Data(void)
	{
		TcpClientPointer pTcpClient = shared_from_this();
		while (m_socket != 0)
		{
			ReceiveBuffer::pointer buffer;
			if (!m_datas.front(buffer))
			{
#ifdef WIN32
				Sleep(10);
#else
				usleep(10000);
#endif
				continue;
			}

			if (m_handler.get() != NULL)
			{
				//m_handler->OnReceiveData(*this, buffer);
				try
				{
					m_handler->OnReceiveData(pTcpClient, buffer);
				}catch(const std::exception&)
				{
				}catch(...)
				{}
			}

			if (m_unused.size() < m_unusedsize)
			{
				buffer->reset();
				m_unused.add(buffer);
			}
		}
	}

	void setHandler(TcpClient_Handler::pointer v) {m_handler = v;}
	void async_read_some(void)
	{
		if (m_socket == 0 || m_proc_data==NULL) return;

		try
		{
			ReceiveBuffer::pointer newBuffer;
			if (!m_unused.front(newBuffer))
				newBuffer = ReceiveBuffer::create();
			m_socket->async_read_some(boost::asio::buffer(const_cast<unsigned char*>(newBuffer->data()), m_maxbuffersize),
				boost::bind(&TcpClient::read_some_handler,this, newBuffer,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
				);
		}catch(const std::exception&)
		{
		}catch(const boost::exception&)
		{
		}catch(...)
		{}
	}

private:

	void connect_handler(const boost::system::error_code& error)
	{
		if (m_socket == 0) return;

		if(!error)
		{
#ifdef USES_OPENSSL
			if (m_socket->is_ssl())
			{
				m_socket->get_ssl_socket()->async_handshake(boost::asio::ssl::stream_base::client,boost::bind(&TcpClient::handle_handshake,
					this,shared_from_this(),boost::asio::placeholders::error));
				return;
			}
#endif
			if (m_handler.get() != NULL)
			{
				//m_handler->OnConnected(*this);
				m_handler->OnConnected(shared_from_this());
			}
			async_read_some();
		}else
		{
			m_socket->close();
			if (m_handler.get() != NULL)
			{
				//m_handler->OnConnectError(*this, error);
				m_handler->OnConnectError(shared_from_this(), error);
			}
		}
	}
	void read_some_handler(ReceiveBuffer::pointer newBuffer, const boost::system::error_code& error, std::size_t size)
	{
		if (m_socket == 0) return;
		// ???·µ»Ø2´íÎó£»

		if(!error)
		{
			newBuffer->size(size);
			m_datas.add(newBuffer);
			//if (m_handler)
			//	m_handler->OnReceiveData(*this, newBuffer);
			async_read_some();
		}else
		{
			// ??
			if (m_handler.get() != NULL)
				m_handler->OnDisconnect(shared_from_this(), error);
			m_socket->close();
		}
	}
	//void handle_write(const boost::system::error_code& error)
	//{
	//}

	static void do_proc_data(TcpClient * owner)
	{
		BOOST_ASSERT (owner != 0);

		owner->proc_Data();
	}

public:
	TcpClient(const TcpClient_Handler::pointer& handler)
		: m_socket(0)
		, m_handler(handler)
		, m_proc_data(0)
		, m_unusedsize(10), m_maxbuffersize(Max_ReceiveBuffer_ReceiveSize)
	{
	}
	virtual ~TcpClient(void)
	{
		disconnect();
		m_handler.reset();
	}
private:
#ifdef USES_OPENSSL
	boost_socket_base<tcp::socket>* m_socket;
#else
    tcp::socket * m_socket;
#endif
	TcpClient_Handler::pointer m_handler;
	boost::thread * m_proc_data;
	CLockList<ReceiveBuffer::pointer> m_datas;
	CLockList<ReceiveBuffer::pointer> m_unused;
	size_t m_unusedsize;
	size_t m_maxbuffersize;
	//tstring m_sSSLPassword;
};

#endif // __TcpClient_h__
