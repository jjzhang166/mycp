#include "MysqlPool.h"

//#if (USES_MYSQLCDBC)

CMysqlSink::CMysqlSink(void)
: m_nPort(3306)
, m_mysql(NULL)
, m_busy(false)

{
}

CMysqlSink::~CMysqlSink(void)
{
	Disconnect();
}

bool CMysqlSink::Init(void)
{
	if (m_mysql==NULL)
		m_mysql = mysql_init((MYSQL*)NULL);
	if (m_mysql==NULL)
		printf("**** mysql_init error.\n");
	return m_mysql!=NULL?true:false;
}

bool CMysqlSink::Connect(void)
{
	if (!Init()) return false;

	if (!mysql_real_connect(m_mysql, m_sHost.c_str(), m_sAccount.c_str(),
		m_sSecure.c_str(), m_sDatabase.c_str(), m_nPort, NULL, CLIENT_MULTI_STATEMENTS))
	{
		printf("**** mysql_real_connect error.(%s)\n",mysql_error(m_mysql));
		mysql_close(m_mysql);
		m_mysql = NULL;
		return false;
	}

	if (m_sCharset.empty())
	{
		mysql_query(m_mysql, "set names utf8;");
	}else
	{
		char lpszCharsetCommand[100];
		sprintf(lpszCharsetCommand, "set names %s;", m_sCharset.c_str());
		mysql_query(m_mysql, lpszCharsetCommand);
	}
	return true;
}
void CMysqlSink::Disconnect(void)
{
	if (m_mysql!=NULL)
	{
		mysql_close(m_mysql);
		m_mysql = NULL;
	}
}
bool CMysqlSink::IsIdleAndSetBusy(void)
{
	BoostWriteLock wtlock(m_mutex);
	if (m_busy)
		return false;
	m_busy = true;
	return true;
}
void CMysqlSink::SetIdle(void)
{
	BoostWriteLock wtlock(m_mutex);
	m_busy = false;
}


CMysqlPool::CMysqlPool(void)
: m_nSinkSize(0),m_nSinkMin(0),m_nSinkMax(0)
, m_sinks(NULL)

{
}

CMysqlPool::~CMysqlPool(void)
{
	PoolExit();
}

bool CMysqlPool::IsOpen(void) const
{
	return m_sinks==NULL?false:true;
}
int CMysqlPool::PoolInit(int nMin, int nMax,const char* lpHost,unsigned int nPort,const char* lpAccount,const char* lpSecure,const char* lpszDatabase,const char* lpCharset)
{
	BoostReadLock rdlock(m_mutex);
	if (m_sinks!=NULL)
	{
		return m_nSinkSize;
	}
	int result = 0;
	m_nSinkSize = nMin;
	m_nSinkMin = nMin;
	m_nSinkMax = nMax;
    m_sinks = (CMysqlSink**)malloc(sizeof(CMysqlSink*) * nMax);
	for(int i = 0; i < m_nSinkMax; i++)
	{
		m_sinks[i] = new CMysqlSink();
		m_sinks[i]->m_sHost = lpHost;
		m_sinks[i]->m_nPort = nPort;
		m_sinks[i]->m_sAccount = lpAccount;
		m_sinks[i]->m_sSecure = lpSecure;
		m_sinks[i]->m_sDatabase = lpszDatabase;
		m_sinks[i]->m_sCharset = lpCharset;
		if (i<m_nSinkMin)
		{
			//m_sinks[i]->Init();
			if (m_sinks[i]->Connect())
			{
				result++;
			}
		}
	}
	return result;
}

void CMysqlPool::PoolExit(void)
{
	m_nSinkSize = 0;
	m_nSinkMin = 0;
	m_nSinkMax = 0;
	BoostReadLock rdlock(m_mutex);
	if (m_sinks!=NULL)
	{
		for(int i = 0; i < m_nSinkMax; i++)
		{
			delete m_sinks[i];
		}
		free(m_sinks);
		m_sinks = NULL;
	}
}

CMysqlSink* CMysqlPool::SinkGet(void)
{
	int nosinkcount = 0;
	static int theminnumbercount = 0;
	while(true)
	{
		{
			BoostReadLock rdlock(m_mutex);
			for (int i=0; i<m_nSinkSize; i++)
			{
				CMysqlSink* pSink = m_sinks[i];
				if (!pSink->IsIdleAndSetBusy())
					continue;

				rdlock.unlock();
				if (m_nSinkSize>m_nSinkMin && i<=(m_nSinkMin-2))	// 只使用了最小值后面二条
				{
					theminnumbercount++;
					if ((i<(m_nSinkMin-2) && theminnumbercount>=30) ||	// 连接30个，只用到最小连接数，减少一个数据库连接
						theminnumbercount>=100)								// 连续100个正常数据库连接低于最低连接数，减少一个数据库连接；
					{
						if (SinkDel())
						{
							theminnumbercount = 0;
							printf("*********** SinkDel OK size:%d\n",m_nSinkSize);
						}
					}
				}else
				{
					theminnumbercount = 0;
				}
				return pSink;
			}
		}

		if (m_nSinkSize==0 || (nosinkcount++)>25)	// 全部忙，并且等25次以上（25*10=250ms左右），增加一个数据库连接，（**25这个值不能太低，低了不好；**)
		{
			nosinkcount = 0;	// 如果找不到还可以重新开始等
			CMysqlSink* pSink = SinkAdd();
			if (pSink)
			{
				printf("*********** SinkAdd OK size:%d\n",m_nSinkSize);
				return pSink;
			}
		}

		//等待一段时间
#ifdef WIN32
		Sleep (10);
#else
		usleep( 10000);
#endif
	}
	return NULL;
}

void CMysqlPool::SinkPut(CMysqlSink* pMysql)
{
	if (pMysql!=NULL)
		pMysql->SetIdle();
}

CMysqlSink* CMysqlPool::SinkAdd(void)
{
	BoostWriteLock wtlock(m_mutex);
	if (m_nSinkSize>=m_nSinkMax)
	{
		return NULL;
	}

	CMysqlSink* pSink = m_sinks[m_nSinkSize];
	if (!pSink->IsIdleAndSetBusy())
	{
		return NULL;
	}
	if (!pSink->Connect())
	{
		pSink->SetIdle();
		return NULL;
	}
	m_nSinkSize++;
	return pSink;
}
bool CMysqlPool::SinkDel(void)
{
	BoostWriteLock wtlock(m_mutex);
	if (m_nSinkSize<=m_nSinkMin)
	{
		return true;
	}
	CMysqlSink* pSink = m_sinks[m_nSinkSize-1];
	if (!pSink->IsIdleAndSetBusy())
	{
		return false;
	}
	pSink->Disconnect();
	pSink->SetIdle();
	m_nSinkSize--;
	return true;
}



//#endif
