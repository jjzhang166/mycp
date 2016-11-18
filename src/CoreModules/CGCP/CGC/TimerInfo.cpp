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
#include <winsock2.h>
#include <Windows.h>
#else
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#endif
#include "TimerInfo.h"

#define USES_ONE_SHOT_NEWVERSION

//
// TimerInfo class 
TimerInfo::TimerInfo(unsigned int nIDEvent, unsigned int nElapse, const cgcOnTimerHandler::pointer& handler, bool bOneShot, const void * pvParam)
: m_nIDEvent(nIDEvent), m_nElapse(nElapse)
, m_timerHandler(handler), m_bOneShot(bOneShot)
, m_pvParam(pvParam)
, m_timerState(TS_Init)

{
	ftime(&m_tLastRunTime);
}

TimerInfo::~TimerInfo(void)
{
	m_timerThread.reset();
	//if (m_timerThread.g != 0)
	//	delete m_timerThread;
}

void TimerInfo::PauseTimer(void)
{
	m_timerState = TS_Pause;
}

void TimerInfo::do_timer(void)
{
	try
	{
		while (getTimerState() != TimerInfo::TS_Exit)
		{
			if (getTimerState() == TimerInfo::TS_Running)
			{
				doRunTimer();
			}
		}

		doTimerExit();
	}catch (const boost::exception &)
	{
	}catch (const boost::thread_exception &)
	{
	}catch (const std::exception& e)
	{
		//
		printf("!!!! TimerInfo::do_timer exception: %s\n",e.what());
	}catch (...)
	{
		//
	}
#ifdef USES_ONE_SHOT_NEWVERSION
	if (m_bOneShot)
		delete this;
#endif
}

void TimerInfo::RunTimer(void)
{
	m_timerState = TS_Running;
	ftime(&m_tLastRunTime);
	if (m_timerThread.get() == NULL)
	{
		try
		{
			//class HelloWorld
			//{
			//public:
			//	void hello()
			//	{
			//		std::cout <<
			//			"Hello world, I''m a thread!"
			//			<< std::endl;
			//	}
			//	void start()
			//	{
			//		boost::function0< void> f =  boost::bind(&HelloWorld::hello,this);
			//		boost::thread thrd( f );
			//		thrd.join();
			//	}
			//}; 
			/*class HelloWorld
			{
			public:
				void hello(const std::string& str)
				{
					std::cout < }
			}; 
			HelloWorld obj;
			boost::thread thrd( boost::bind(&HelloWorld::hello,&obj,"Hello 
				world, I''m a thread!" ) ) ;*/
			//boost::function0< void> f =  boost::bind(&TimerInfo::do_timer,this);
			//boost::thread thrd( f );

			boost::thread_attributes attrs;
			attrs.set_stack_size(CGC_THREAD_STACK_MIN);
			//boost::thread thrd1( boost::bind(&TimerInfo::do_timer,this) );
			//boost::thread thrd2(attrs,boost::bind(&TimerInfo::do_timer,this));
			m_timerThread = boost::shared_ptr<boost::thread>(new boost::thread(attrs,boost::bind(&TimerInfo::do_timer, this)));
		}catch (const boost::exception &)
		{
			m_timerState = TS_Exit;
		}catch (const boost::thread_exception &)
		{
			m_timerState = TS_Exit;
		}catch (const std::exception &)
		{
			m_timerState = TS_Exit;
		}catch (...)
		{
			m_timerState = TS_Exit;
		}
	}
}

void TimerInfo::KillTimer(void)
{
	m_timerState = TS_Exit;
	if (m_bOneShot) return;	// add by hd 2016-08-27
	//boost::mutex::scoped_lock lockTimer(m_mutex);
	//m_timerHandler.reset();
	boost::shared_ptr<boost::thread> timerThreadTemp = m_timerThread;
	m_timerThread.reset();
	if (timerThreadTemp.get()!=NULL)
	{
		//if (!m_bOneShot)
			timerThreadTemp->join();
		timerThreadTemp.reset();
	}
}

void TimerInfo::doRunTimer(void)
{
	struct timeb tNow;
	ftime(&tNow);

	//
	// the system time error.
	if (tNow.time < m_tLastRunTime.time)
	{
		m_tLastRunTime = tNow;
		return;
	}

	//
	// compare the second
	if (tNow.time < m_tLastRunTime.time + m_nElapse/1000)
	{
#ifdef WIN32
		Sleep(100);
#else
		usleep(100000);
#endif
		return;
	}

	//
	// compare the millisecond
	if (tNow.time == m_tLastRunTime.time + m_nElapse/1000)
	{
		const long nOff = m_tLastRunTime.millitm+(m_nElapse%1000) - tNow.millitm;
		if (nOff > 1)	// 0
		{
#ifdef WIN32
			Sleep(nOff);
#else
			usleep(nOff*1000);
#endif
		}
	}

	// OnTimer
	if (m_timerHandler.get() != NULL)
	{
		boost::mutex::scoped_lock * lock = NULL;
		try
		{
			if (m_timerHandler->IsThreadSafe())
				lock = new boost::mutex::scoped_lock(m_timerHandler->GetMutex());
			ftime(&m_tLastRunTime);
			m_timerHandler->OnTimeout(m_nIDEvent, m_pvParam);
		}catch (const boost::exception &)
		{
		}catch (const boost::thread_exception &)
		{
		}catch (const std::exception & e)
		{
			printf("******* timeout exception: %s\n",e.what());
		}catch(...)
		{
			printf("******* timeout exception.\n");
		}
		if (lock != NULL)
			delete lock;
	}
	if (m_bOneShot)
	{
		KillTimer();
		return;
	}
	//ftime(&m_tLastRunTime);
	if (m_nElapse < 100)
	{
//#ifdef WIN32
//		Sleep(2);
//#else
//		usleep(2000);
//#endif
		return;
	}

#ifdef WIN32
	Sleep(m_nElapse > 1000 ? 1000 : (m_nElapse-5));
#else
	usleep(m_nElapse > 1000 ? 1000000 : (m_nElapse-5)*1000);
#endif
}

void TimerInfo::doTimerExit(void)
{
	if (m_timerHandler.get() != NULL)
		m_timerHandler->OnTimerExit(m_nIDEvent, m_pvParam);
}


//
// TimerTable class
TimerTable::TimerTable(void)
{

}

TimerTable::~TimerTable(void)
{
	KillAll();
}


unsigned int TimerTable::SetTimer(unsigned int nIDEvent, unsigned int nElapse, const cgcOnTimerHandler::pointer& handler, bool bOneShot, const void * pvParam)
{
	if (nIDEvent <= 0 || nElapse <= 0 || handler.get() == NULL) return 0;

#ifdef USES_ONE_SHOT_NEWVERSION
	if (bOneShot)
	{
		TimerInfo * pTimerInfo = new TimerInfo(nIDEvent, nElapse, handler, bOneShot, pvParam);
		pTimerInfo->RunTimer();
		return (unsigned int)pTimerInfo;
	}
#endif

	TimerInfo * pTimerInfo = m_mapTimerInfo.find(nIDEvent, false);
	if (pTimerInfo == 0)
	{
		pTimerInfo = new TimerInfo(nIDEvent, nElapse, handler, bOneShot, pvParam);
		m_mapTimerInfo.insert(nIDEvent, pTimerInfo);
	}else
	{
		pTimerInfo->PauseTimer();
		pTimerInfo->setElapse(nElapse);
		pTimerInfo->setOneShot(bOneShot);
		pTimerInfo->setTimerHandler(handler);
		pTimerInfo->setParam(pvParam);
	}

	pTimerInfo->RunTimer();
	return (unsigned int)pTimerInfo;
	//return true;
}

void TimerTable::KillTimer(unsigned int nIDEvent)
{
	TimerInfo * pTimerInfo = m_mapTimerInfo.find(nIDEvent, true);
	if (pTimerInfo != 0)
	{
		pTimerInfo->KillTimer();
		delete pTimerInfo;
	}
}

void TimerTable::KillAll(void)
{
	AUTO_LOCK(m_mapTimerInfo);
	for_each(m_mapTimerInfo.begin(), m_mapTimerInfo.end(),
		boost::bind(&TimerInfo::KillTimer, boost::bind(&std::map<unsigned int, TimerInfo*>::value_type::second,_1)));
	m_mapTimerInfo.clear(false, true);
	/*CLockMapPtr<unsigned int, TimerInfo*>::iterator pIter;
	for (pIter=m_mapTimerInfo.begin(); pIter!=m_mapTimerInfo.end(); pIter++)
	{
		TimerInfo * pTimerInfo = pIter->second;
		pTimerInfo->KillTimer();
		delete pTimerInfo;
	}
	m_mapTimerInfo.clear(false, false);*/
}
