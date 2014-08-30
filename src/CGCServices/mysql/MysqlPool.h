// MysqlPool.h file here
#ifndef __MysqlPool_h__
#define __MysqlPool_h__

//#if (USES_MYSQLCDBC)
#ifdef WIN32
#pragma warning(disable:4267)
#include <windows.h>
#endif

#ifdef WIN32
#include <mysql.h>
#pragma comment(lib,"libmysql.lib")
#else
#include "mysql/mysql.h"
#endif // WIN32
#include <stl/lockmap.h>
#include <string>

class CMysqlSink
{
public:
	CMysqlSink(void);
	virtual ~CMysqlSink(void);
	std::string m_sHost;
	std::string m_sAccount;
	std::string m_sSecure;
	std::string m_sDatabase;
	std::string m_sCharset;

	bool Connect(void);
	void Disconnect(void);

	bool IsIdleAndSetBusy(void);
	void SetIdle(void);
	MYSQL* GetMYSQL(void) {return m_mysql;}

private:
	bool Init(void);

private:
	MYSQL* m_mysql;
	bool m_busy;
	boost::shared_mutex m_mutex; 
};

class CMysqlPool
{
public:
	CMysqlPool(void);
	virtual ~CMysqlPool(void);

	bool IsOpen(void) const;
	int PoolInit(int nMin, int nMax,const char* lpHost,const char* lpAccount,const char* lpSecure,const char* lpszDatabase,const char* lpCharset);
	void PoolExit(void);

	CMysqlSink* SinkGet(void);
	void SinkPut(CMysqlSink* pMysql);

private:
	CMysqlSink* SinkAdd(void);
	bool SinkDel(void);

private:
	int m_nSinkSize;
	int m_nSinkMin;
	int m_nSinkMax;
	boost::shared_mutex m_mutex; 
	CMysqlSink** m_sinks;
	
};
//#endif

#endif //__MysqlPool_h__
