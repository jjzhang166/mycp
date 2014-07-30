// boost_socket.h file here
#ifndef __boost_socket_h__
#define __boost_socket_h__

// *** must install openssl
#define USES_OPENSSL

#ifdef USES_OPENSSL
#include <boost/asio/ssl.hpp>
#endif

template<typename T>
class boost_socket_base
{
protected:
    boost_socket_base()
    {}
public:
	virtual bool is_ssl(void) const = 0;
#ifdef USES_OPENSSL
	virtual boost::asio::ssl::stream<T>* get_ssl_socket(void) {return NULL;}
#endif
	virtual T* get_socket(void) {return NULL;}

	void close(void)
	{
		lowest_layer().close();
	}
	typename T::lowest_layer_type& lowest_layer(void)
	{
#ifdef USES_OPENSSL
		if(is_ssl()) 
			return get_ssl_socket()->lowest_layer();
		else
#endif
			return get_socket()->lowest_layer();
	}

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers)
    {
#ifdef USES_OPENSSL
		if(is_ssl()) 
            return get_ssl_socket()->read_some(buffers);
        else
#endif
            return get_socket()->read_some(buffers);
    }
    
    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers,boost::system::error_code& ec)
    {
#ifdef USES_OPENSSL
		if(is_ssl()) 
            return get_ssl_socket()->read_some(buffers,ec);
        else
#endif
            return get_socket()->read_some(buffers,ec);
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read_some(const MutableBufferSequence& buffers,BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
    {
#ifdef USES_OPENSSL
		//printf("**** boost_socket async_read_some,is_ssl=%d,%x\n",is_ssl()?1:0,get_ssl_socket());
		if(is_ssl())
            get_ssl_socket()->async_read_some(buffers,handler);
        else
#endif
			get_socket()->async_read_some(buffers,handler);
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers,boost::system::error_code& ec)
    {
#ifdef USES_OPENSSL
		if(is_ssl())
            return get_ssl_socket()->write_some(buffers,ec);
        else
#endif
            return get_socket()->write_some(buffers,ec);
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers)
    {
#ifdef USES_OPENSSL
		if(is_ssl())
            return get_ssl_socket()->write_some(buffers);
        else
#endif
            return get_socket()->write_some(buffers);
    }

   template <typename ConstBufferSequence>
    std::size_t write(const ConstBufferSequence& buffers,boost::system::error_code& ec)
    {
#ifdef USES_OPENSSL
		if(is_ssl())
            return get_ssl_socket()->write(buffers,ec);
        else
#endif
            return get_socket()->write(buffers,ec);
    }

    template <typename ConstBufferSequence>
    std::size_t write(const ConstBufferSequence& buffers)
    {
#ifdef USES_OPENSSL
		if(is_ssl())
            return get_ssl_socket()->write(buffers);
        else
#endif
            return get_socket()->write(buffers);
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_write_some(const MutableBufferSequence& buffers,BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
    {    
#ifdef USES_OPENSSL
		if(is_ssl())
            get_ssl_socket()->async_write_some(buffers,handler);
        else
#endif
            get_socket()->async_write_some(buffers,handler);
    }
public:
    virtual ~boost_socket_base() {}
};

template<typename T>
class boost_socket : public boost_socket_base<T>
{
public:
    boost_socket(boost::asio::io_service& io_service)
		: s_(io_service)
    {
	}
public:
    virtual ~boost_socket() {}
private:
	virtual bool is_ssl(void) const {return false;}
	virtual T* get_socket(void) {return &s_;}
	T s_;
};

#ifdef USES_OPENSSL
template<typename T>
class boost_ssl_socket : public boost_socket_base<T>
{
public:
    boost_ssl_socket(boost::asio::io_service& io_service,boost::asio::ssl::context& ctx)
		: ss_(io_service,ctx)
    {
	}
public:
    virtual ~boost_ssl_socket() {}

private:
	virtual bool is_ssl(void) const {return true;}
	virtual boost::asio::ssl::stream<T>* get_ssl_socket(void) {return &ss_;}
    boost::asio::ssl::stream<T> ss_;
};
#endif

#endif // __boost_socket_h__
