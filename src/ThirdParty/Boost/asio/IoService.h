// IoService.h file here
#ifndef __IoService_h__
#define __IoService_h__

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
//
//#ifdef WIN32
//#include <Mmsystem.h>
//#else
//#include <time.h>   
//inline unsigned long timeGetTime()  
//{  
//	unsigned long uptime = 0;  
//	struct timespec on;  
//	if(clock_gettime(CLOCK_MONOTONIC, &on) == 0)  
//		uptime = on.tv_sec*1000 + on.tv_nsec/1000000;  
//	return uptime;  
//} 
//#endif

class IoService_Handler
{
public:
	typedef boost::shared_ptr<IoService_Handler> pointer;
	virtual void OnIoServiceException(void)=0;
};
const IoService_Handler::pointer ConstNullIoService_Handler;

///////////////////////////////////////////////
// IoService class
class IoService
{
public:
	typedef boost::shared_ptr<IoService> pointer;
	static IoService::pointer create(void) {return IoService::pointer(new IoService());}

	void start(IoService_Handler::pointer handler=ConstNullIoService_Handler)
	{
		m_bKilled = false;
		m_pHandler = handler;
		//m_ioservice.reset();
		if (m_pProcEventLoop == NULL)
			m_pProcEventLoop = new boost::thread(boost::bind(&IoService::do_event_loop, this));
	}

	void stop(void)
	{
		m_bKilled = true;
		m_ioservice.stop();
		if (m_pProcEventLoop)
		{
			m_pProcEventLoop->join();
			delete m_pProcEventLoop;
			m_pProcEventLoop = NULL;
		}
	}
	bool is_killed(void) const {return m_bKilled;}
	bool is_start(void) const {return (m_pProcEventLoop != NULL);}
	boost::asio::io_service & ioservice(void) {return m_ioservice;}

	void on_exception(void)
	{
		if (m_pHandler.get())
			m_pHandler->OnIoServiceException();
	}
private:
	static void do_event_loop(IoService * iopservice)
	{
		if (iopservice)
		{
			boost::asio::io_service & pService = iopservice->ioservice();
			//boost::asio::io_service::work work(pService);	// 保证run不会退出
			while (!iopservice->is_killed())
			{
				try
				{
#ifdef WIN32
					Sleep(2);
#else
					usleep(2000);
#endif
					pService.poll();
					continue;
					boost::system::error_code ec;
					pService.run(ec);
					pService.reset();
					printf("do_event_loop exit:%s=%d\n",ec.message().c_str(),ec.value());
					break;
				}catch (std::exception & e)
				{
					pService.reset();
#ifdef WIN32
					printf("do_event_loop exception. %s, lasterror=%d\n", e.what(), GetLastError());
#else
					printf("do_event_loop exception. %s\n", e.what());
#endif
					//const std::string sMessage = e.what();
					//if (sMessage.find("remote_endpoint: Transport endpoint is not connected")==0)
					//{
					//	continue;
					//}
					iopservice->on_exception();
					break;
				}
			}

		}
	}

public:
	IoService(void)
		: m_bKilled(false), m_pProcEventLoop(NULL)
	{}
	virtual ~IoService(void)
	{
		stop();
	}
private:
	bool m_bKilled;
	boost::asio::io_service m_ioservice;
	boost::thread * m_pProcEventLoop;
	IoService_Handler::pointer m_pHandler;
};

#endif // __IoService_h__
