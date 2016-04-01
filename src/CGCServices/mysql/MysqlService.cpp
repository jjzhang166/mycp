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

#define USES_MYSQLCDBC		1		// [0,1]

#ifdef WIN32
#pragma warning(disable:4267)
#include <windows.h>
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

//#pragma comment(lib, "mysqlcppconn.lib")
//#pragma comment(lib, "mysqlcppconn-static.lib")

#endif // WIN32

#include <CGCBase/cdbc.h>
using namespace cgc;

#if (USES_MYSQLCDBC)

#include "MysqlPool.h"
//#include <mysql.h>
//#ifdef WIN32
//#pragma comment(lib,"libmysql.lib")
//#endif // WIN32

my_ulonglong GetRowsNumber(MYSQL_RES * res)
{
	return mysql_num_rows(res);
}

class CCDBCResultSet
{
public:
	typedef boost::shared_ptr<CCDBCResultSet> pointer;

	cgc::bigint size(void) const {return m_rows;}
	cgc::bigint cols(void) const {return m_fields;}
	cgc::bigint index(void) const
	{
		//return m_resultset == NULL ? -1 : (int)m_resultset->getRow();
		return m_currentIndex;
	}
	cgcValueInfo::pointer cols_name(void) const
	{
		if (m_resultset == NULL || m_rows == 0 || m_resultset->field_count==0) return cgcNullValueInfo;
	
		std::vector<cgcValueInfo::pointer> record;
		try
		{
			char lpszFieldName[12];
			for(unsigned int i=0; i<m_resultset->field_count; i++)
			{
				MYSQL_FIELD * pField = mysql_fetch_field(m_resultset);
				if (pField!=NULL && pField->name!=NULL)
					record.push_back(CGC_VALUEINFO(pField->name));
				else
				{
					sprintf(lpszFieldName,"%d",i);
					record.push_back(CGC_VALUEINFO(lpszFieldName));
				}
			}
		}catch(std::exception&)
		{
		}catch(...)
		{
			// ...
		}
		return CGC_VALUEINFO(record);
	}
	cgcValueInfo::pointer index(cgc::bigint moveIndex)
	{
		if (m_resultset == NULL || m_rows == 0) return cgcNullValueInfo;
		if (moveIndex < 0 || (moveIndex+1) > m_rows) return cgcNullValueInfo;
		//if (!m_resultset->relative(moveIndex)) return cgcNullValueInfo;

		m_currentIndex = moveIndex;
		return getCurrentRecord();
	}
	cgcValueInfo::pointer first(void)
	{
		if (m_resultset == NULL || m_rows == 0) return cgcNullValueInfo;
		//if (!m_resultset->first()) return cgcNullValueInfo;

		m_currentIndex = 0;
		return getCurrentRecord();
	}
	cgcValueInfo::pointer next(void)
	{
		if (m_resultset == NULL || m_rows == 0) return cgcNullValueInfo;
		if (m_currentIndex+1 == m_rows) return cgcNullValueInfo;
		//if (!m_resultset->next()) return cgcNullValueInfo;

		++m_currentIndex;
		return getCurrentRecord();
	}
	cgcValueInfo::pointer previous(void)
	{
		if (m_resultset == NULL || m_rows == 0) return cgcNullValueInfo;
		if (m_currentIndex == 0) return cgcNullValueInfo;
		//if (!m_resultset->previous()) return cgcNullValueInfo;

		--m_currentIndex;
		return getCurrentRecord();
	}
	cgcValueInfo::pointer last(void)
	{
		if (m_resultset == NULL || m_rows == 0) return cgcNullValueInfo;
		//if (!m_resultset->last()) return cgcNullValueInfo;
		m_currentIndex = m_rows - 1;
		return getCurrentRecord();
	}
	void reset(void)
	{
		if (m_resultset)
		{
			try
			{
				mysql_free_result(m_resultset);
			}catch(std::exception&)
			{
			}catch(...)
			{}
			CMysqlPool::SinkPut(m_pSink);
			m_resultset = NULL;
		}
		m_rows = 0;
		//try
		//{
		//	if (m_resultset)
		//	{
		//		m_resultset->close();
		//		delete m_resultset;
		//		m_resultset = NULL;
		//	}
		//	if (m_stmt)
		//	{
		//		delete m_stmt;
		//		m_stmt = NULL;
		//	}
		//}catch(sql::SQLException &e)
		//{
		//}
		//m_currentIndex = 0;
	}

	//CCDBCResultSet(sql::ResultSet * resultset, sql::Statement * stmt)
	//	: m_resultset(resultset), m_stmt(stmt)//, m_currentIndex(0)
	//{}
	CCDBCResultSet(CMysqlSink* pSink,MYSQL_RES * resultset)
		: m_pSink(pSink),m_resultset(resultset), m_rows(0)
	{
		m_rows = mysql_num_rows(m_resultset);
	}
	virtual ~CCDBCResultSet(void)
	{
		reset();
	}

protected:
	cgcValueInfo::pointer getCurrentRecord(void) const
	{
		assert (m_resultset != NULL);
		assert (m_currentIndex >= 0 && m_currentIndex < m_rows);
		//if (m_resultset->isBeforeFirst() || m_resultset->isAfterLast() || m_resultset->isClosed())
		//	return cgcNullValueInfo;

		std::vector<cgcValueInfo::pointer> record;
		try
		{
			mysql_data_seek(m_resultset, (my_ulonglong)m_currentIndex);
			MYSQL_ROW_OFFSET rowoffset = mysql_row_tell(m_resultset);
			if (rowoffset == NULL)
				return cgcNullValueInfo;
			//MYSQL_ROW row = mysql_fetch_row(m_resultset);
			//MYSQL_ROW row = m_resultset->current_row;
			MYSQL_ROW row = rowoffset->data;
			if (row != NULL)
			{
				for(unsigned int i=0; i<m_resultset->field_count; i++)
				{
					tstring s = row[i] ? (const char*)row[i] : "";
					record.push_back(CGC_VALUEINFO(s));
				}
			}
		}catch(std::exception&)
		{
		}catch(...)
		{
			// ...
		}

		//for (int i=1; i<35; i++)
		//{
		//	try
		//	{
		//		//if (m_resultset->isNull(i))
		//		//	break;
		//		//int s = m_resultset->getInt(i);
		//		sql::SQLString s = m_resultset->getString(i);
		//		record.push_back(CGC_VALUEINFO(s));
		//	} catch (sql::SQLException &e)
		//	{
		//		break;
		//	}
		//}
		return CGC_VALUEINFO(record);
	}

private:
	CMysqlSink * m_pSink;
	MYSQL_RES * m_resultset;
	cgc::bigint		m_rows;
	int			m_fields;
	//sql::ResultSet *	m_resultset;
	//sql::Statement *	m_stmt;
	cgc::bigint		m_currentIndex;
};

#define CDBC_RESULTSET(s,r) CCDBCResultSet::pointer(new CCDBCResultSet(s,r))

const int escape_in_size = 2;
const tstring escape_in[] = {"''","\\\\"};
const tstring escape_out[] = {"'","\\"};
const int escape_old_out_size = 2;	// ?兼容旧版本
const tstring escape_old_in[] = {"&lsquo;","&pge0;"};
const tstring escape_old_out[] = {"'","\\"};

//const int escape_in_size = 2;
//const int escape_out_size = 4;	// ?兼容旧版本
//const tstring escape_in[] = {"\'","\\\\","&lsquo;","&pge0;"};
//const tstring escape_out[] = {"'","\\","'","\\"};
//const int escape_size = 2;
//const std::string escape_in[] = {"&lsquo;","&mse0;"};
//const std::string escape_out[] = {"'","\\"};


class CMysqlCdbc
	: public cgcCDBCService
{
public:
	typedef boost::shared_ptr<CMysqlCdbc> pointer;

	CMysqlCdbc(const tstring& path)
		//: m_driver(NULL), m_con(NULL)
		//: m_mysql(NULL)
		: m_tLastTime(0)
		, m_nLastErrorCode(0), m_tLastErrorTime(0)

	{}
	virtual ~CMysqlCdbc(void)
	{
		finalService();
	}
	virtual bool initService(cgcValueInfo::pointer parameter)
	{
		if (isServiceInited()) return true;

		//try
		//{
		//	if (m_driver == NULL)
		//		m_driver = get_driver_instance();
		//	if (m_driver == NULL)
		//		return false;
		//} catch (sql::SQLException &e)
		//{
		//	return false;
		//}

		m_bServiceInited = true;
		return isServiceInited();
	}
	virtual void finalService(void)
	{
		close();

		//m_driver = NULL;
		m_cdbcInfo.reset();
		m_bServiceInited = false;
	}
private:
	virtual tstring serviceName(void) const {return _T("MYSQLCDBC");}

	static const tstring & replace(tstring & strSource, const tstring & strFind, const tstring &strReplace)
	{
		std::string::size_type pos=0;
		std::string::size_type findlen=strFind.size();
		std::string::size_type replacelen=strReplace.size();
		while ((pos=strSource.find(strFind, pos)) != std::string::npos)
		{
			strSource.replace(pos, findlen, strReplace);
			pos += replacelen;
		}
		return strSource;
	}

	virtual void escape_string_in(tstring & str)
	{
		for (int i=0; i<escape_in_size; i++)
			replace(str, escape_out[i], escape_in[i]);
	}
	virtual void escape_string_out(tstring & str)
	{
		for (int i=0; i<escape_old_out_size; i++)
			replace(str, escape_old_in[i], escape_old_out[i]);
	}

	virtual bool open(const cgcCDBCInfo::pointer& cdbcInfo)
	{
		if (cdbcInfo.get() == NULL) return false;
		if (!isServiceInited()) return false;

		if (m_cdbcInfo.get() == cdbcInfo.get() && this->isopen()) return true;

		// close old database;
		close();
		m_cdbcInfo = cdbcInfo;
		m_tLastTime = time(0);

		try
		{
			unsigned short nMin = m_cdbcInfo->getMinSize();
			unsigned short nMax = m_cdbcInfo->getMaxSize();

			int nPort = 3306;
			tstring sHost = m_cdbcInfo->getHost();
			//printf("**** open mysql host=%s\n",sHost.c_str());
			const std::string::size_type find = sHost.find(":");
			if (find != std::string::npos)
			{
				nPort = atoi(sHost.substr(find+1).c_str());	// **必须放前面
				sHost = sHost.substr(0,find);	// ip:port, remove *:port
			}
			//printf("**** open mysql host=%s:%d,account=%s,pwd=%s\n",sHost.c_str(),nPort,m_cdbcInfo->getAccount().c_str(),m_cdbcInfo->getSecure().c_str());
			if (m_mysqlPool.PoolInit(nMin,nMax,
				sHost.c_str(), nPort,
				m_cdbcInfo->getAccount().c_str(),
				m_cdbcInfo->getSecure().c_str(),
				m_cdbcInfo->getDatabase().c_str(),
				m_cdbcInfo->getCharset().c_str())==0)
			{
				//printf("**** open mysql error.(host=%s,account=%s,pwd=%s,db=%s)\n",sHost.c_str(),m_cdbcInfo->getAccount().c_str(),m_cdbcInfo->getSecure().c_str(),m_cdbcInfo->getDatabase().c_str());
				m_mysqlPool.PoolExit();
				return false;
			}
		}catch(std::exception&)
		{
			return false;
		}catch(...)
		{
			return false;
		}

		//try
		//{
		//	//m_con = m_driver->connect("tcp://127.0.0.1:3306", "root", "root");
		//	m_con = m_driver->connect(cdbcInfo->getConnection().c_str(), cdbcInfo->getAccount().c_str(), cdbcInfo->getSecure().c_str());
		//	if (m_con == NULL)
		//		return false;

		//	m_con->setSchema(cdbcInfo->getDatabase().c_str());

		//} catch (sql::SQLException &e)
		//{
		//	m_con = NULL;
		//	return false;
		//}

		return true;
	}
	virtual bool open(void) {return open(m_cdbcInfo);}
	virtual void close(void)
	{
		try
		{
			if (m_mysqlPool.IsOpen())
			{
				m_mysqlPool.PoolExit();
				m_tLastTime = time(0);
			}
			//if (m_mysql)
			//{
			//	mysql_close(m_mysql);
			//	m_mysql = NULL;
			//	m_tLastTime = time(0);
			//}
		}catch(std::exception&)
		{
		}catch(...)
		{
		}
		m_results.clear();
	}
	virtual bool isopen(void) const
	{
		return m_mysqlPool.IsOpen();
		//return m_mysql != NULL;
		//return (m_con != NULL && !m_con->isClosed());
	}
	virtual time_t lasttime(void) const {return m_tLastTime;}

	virtual cgc::bigint execute(const char * exeSql, int nTransaction)
	{
		if (exeSql == NULL || !isServiceInited()) return -1;
		if (!open()) return -1;

		CMysqlSink* pSink = (CMysqlSink*)nTransaction;
		cgc::bigint ret = 0;
		try
		{
			if (CMysqlPool::IsServerError(m_nLastErrorCode) && (m_tLastErrorTime+10)>time(0))
			{
				//if (nTransaction!=0)
				//	trans_rollback(nTransaction);
				return -1;
			}
			if (nTransaction==0)
				pSink = m_mysqlPool.SinkGet(m_nLastErrorCode);
			if (pSink==NULL)
			{
				m_tLastErrorTime = time(0);
				return -1;
			}
			MYSQL * pMysql = pSink->GetMYSQL();
			if(mysql_query(pMysql, exeSql))
			{
				// MySQL server has gone away
				// 2006:MySQL server has gone away
				/*
1005: 创建表失败
1006: 创建数据库失败
1007: 数据库已存在，创建数据库失败
1008: 数据库不存在，删除数据库失败
1009: 不能删除数据库文件导致删除数据库失败
1010: 不能删除数据目录导致删除数据库失败
1011: 删除数据库文件失败
1012: 不能读取系统表中的记录
1016: 无法打开文件
1020:记录已被其他用户修改
1021:硬盘剩余空间不足，请加大硬盘可用空间
1022:关键字重复，更改记录失败
1023:关闭时发生错误
1024:读文件错误
1025:更改名字时发生错误
1026:写文件错误
1032:记录不存在
1036:数据表是只读的，不能对它进行修改
1037:系统内存不足，请重启数据库或重启服务器
1038:用于排序的内存不足，请增大排序缓冲区
1040:已到达数据库的最大连接数，请加大数据库可用连接数
1041:系统内存不足
1042:无效的主机名
1043:无效连接
1044:当前用户没有访问数据库的权限
1045:不能连接数据库，用户名或密码错误
1040: 最大连接数
1048:字段不能为空
1049:数据库不存在
1050:数据表已存在
1051:数据表不存在
1054:字段不存在
1065:无效的SQL语句，SQL语句为空
1081:不能建立Socket连接
1114:数据表已满，不能容纳任何记录
1116:打开的数据表太多
1129:数据库出现异常，请重启数据库
1130:连接数据库失败，没有连接数据库的权限
1133:数据库用户不存在
1141:当前用户无权访问数据库
1142:当前用户无权访问数据表
1143:当前用户无权访问数据表中的字段
1146:数据表不存在
1147:未定义用户对数据表的访问权限
1149:SQL语句语法错误
1158:网络错误，出现读错误，请检查网络连接状况
1159:网络错误，读超时，请检查网络连接状况
1160:网络错误，出现写错误，请检查网络连接状况
1161:网络错误，写超时，请检查网络连接状况
1062:字段值重复，入库失败

                  1162                  	                  ER_TOO_LONG_STRING                 
                  1163                  	                  ER_TABLE_CANT_HANDLE_BLOB                 
                  1164                  	                  ER_TABLE_CANT_HANDLE_AUTO_INCREMENT                 
                  1165                  	                  ER_DELAYED_INSERT_TABLE_LOCKED                 
                  1166                  	                  ER_WRONG_COLUMN_NAME                 
                  1167                  	                  ER_WRONG_KEY_COLUMN                 
                  1168                  	                  ER_WRONG_MRG_TABLE                 
                  1169                  	                  ER_DUP_UNIQUE                 
                  1170                  	                  ER_BLOB_KEY_WITHOUT_LENGTH                 
                  1171                  	                  ER_PRIMARY_CANT_HAVE_NULL                 
                  1172                  	                  ER_TOO_MANY_ROWS                 
                  1173                  	                  ER_REQUIRES_PRIMARY_KEY                 
                  1174                  	                  ER_NO_RAID_COMPILED                 
                  1175                  	                  ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE                 
                  1176                  	                  ER_KEY_DOES_NOT_EXITS                 
                  1177                  	                  ER_CHECK_NO_SUCH_TABLE                 
                  1178                  	                  ER_CHECK_NOT_IMPLEMENTED                 
                  1179                  	                  ER_CANT_DO_THIS_DURING_AN_TRANSACTION                 
                  1180                  	                  ER_ERROR_DURING_COMMIT                 
                  1181                  	                  ER_ERROR_DURING_ROLLBACK                 
                  1182                  	                  ER_ERROR_DURING_FLUSH_LOGS                 
                  1183                  	                  ER_ERROR_DURING_CHECKPOINT                 
                  1184                  	                  ER_NEW_ABORTING_CONNECTION                 
                  1185                  	                  ER_DUMP_NOT_IMPLEMENTED                 
                  1186                  	                  ER_FLUSH_MASTER_BINLOG_CLOSED                 
                  1187                  	                  ER_INDEX_REBUILD                 
                  1188                  	                  ER_MASTER                 
                  1189                  	                  ER_MASTER_NET_READ                 
                  1190                  	                  ER_MASTER_NET_WRITE                 
                  1191                  	                  ER_FT_MATCHING_KEY_NOT_FOUND                 
                  1192                  	                  ER_LOCK_OR_ACTIVE_TRANSACTION                 
                  1193                  	                  ER_UNKNOWN_SYSTEM_VARIABLE                 
                  1194                  	                  ER_CRASHED_ON_USAGE                 
                  1195                  	                  ER_CRASHED_ON_REPAIR                 
                  1196                  	                  ER_WARNING_NOT_COMPLETE_ROLLBACK                 
                  1197                  	                  ER_TRANS_CACHE_FULL                 
                  2000                  	                  CR_UNKNOWN_ERROR                 
                  2001                  	                  CR_SOCKET_CREATE_ERROR                 
                  2002                  	                  CR_CONNECTION_ERROR                 
                  2003                  	                  CR_CONN_HOST_ERROR                 
                  2004                  	                  CR_IPSOCK_ERROR                 
                  2005                  	                  CR_UNKNOWN_HOST                 
                  2006                  	                  CR_SERVER_GONE_ERROR                 
                  2007                  	                  CR_VERSION_ERROR                 
                  2008                  	                  CR_OUT_OF_MEMORY                 
                  2009                  	                  CR_WRONG_HOST_INFO                 
                  2010                  	                  CR_LOCALHOST_CONNECTION                 
                  2011                  	                  CR_TCP_CONNECTION                 
                  2012                  	                  CR_SERVER_HANDSHAKE_ERR                 
                  2013                  	                  CR_SERVER_LOST                 
                  2014                  	                  CR_COMMANDS_OUT_OF_SYNC                 
                  2015                  	                  CR_NAMEDPIPE_CONNECTION                 
                  2016                  	                  CR_NAMEDPIPEWAIT_ERROR                 
                  2017                  	                  CR_NAMEDPIPEOPEN_ERROR                 
                  2018                  	                  CR_NAMEDPIPESETSTATE_ERROR                 
                  2019                  	                  CR_CANT_READ_CHARSET                 
                  2020                  	                  CR_NET_PACKET_TOO_LARGE          
				  */
				//mysql_ping(nMysqlError); // mysql_ping需要定时做心跳
				m_tLastErrorTime = time(0);
				m_nLastErrorCode = mysql_errno(pMysql);
				CGC_LOG((cgc::LOG_WARNING, "%s(%d:%s)\n", exeSql,m_nLastErrorCode,mysql_error(pMysql)));
				if (CMysqlPool::IsServerError(m_nLastErrorCode))
				{
					// **重新连接
					//printf("******* Reconnect 888");
					if (!pSink->Reconnect())
					{
						// **重连失败
						if (nTransaction==0)
							CMysqlPool::SinkPut(pSink);
						//else
						//	trans_rollback(nTransaction);
						return -1;
					}
					// **重连成功
					pMysql = pSink->GetMYSQL();
					if(mysql_query(pMysql, exeSql))
					{
						if (nTransaction==0)
							CMysqlPool::SinkPut(pSink);
						//else
						//	trans_rollback(nTransaction);
						return -1;
					}
					// **重新查询成功
				}else
				{
					// **其他错误
					if (nTransaction==0)
						CMysqlPool::SinkPut(pSink);
					//else
					//	trans_rollback(nTransaction);
					return -1;
				}
			}
			m_nLastErrorCode = 0;
			//CR_COMMANDS_OUT_OF_SYNC
			//CR_SERVER_GONE_ERROR
			//mysql_ping(
			if (nTransaction==0)
			{
				ret = (cgc::bigint)mysql_affected_rows(pMysql);
				MYSQL_RES * result = 0;
				do
				{
					if ( !(result = mysql_store_result(pMysql)))
					{
						break;
					} 
					mysql_free_result(result);
				}while( !mysql_next_result( pMysql ) );
				CMysqlPool::SinkPut(pSink);
			}
		}catch(std::exception&)
		{
			CGC_LOG((cgc::LOG_ERROR, "%s\n", exeSql));
			//printf("******* Reconnect 999");
			if (!pSink->Reconnect())
			{
				CGC_LOG((cgc::LOG_ERROR, "Reconnect error.\n"));
			}
			if (nTransaction==0)
				CMysqlPool::SinkPut(pSink);
			//else
			//	trans_rollback(nTransaction);
			return -1;
		}catch(...)
		{
			CGC_LOG((cgc::LOG_ERROR, "%s\n", exeSql));
			//printf("******* Reconnect aaa");
			if (!pSink->Reconnect())
			{
				CGC_LOG((cgc::LOG_ERROR, "Reconnect error.\n"));
			}
			if (nTransaction==0)
				CMysqlPool::SinkPut(pSink);
			//else
			//	trans_rollback(nTransaction);
			return -1;
		}
		//try
		//{
		//	sql::Statement * stmt = m_con->createStatement();
		//	ret = stmt->executeUpdate(exeSql);
		//	delete stmt;
		//} catch (sql::SQLException &e)
		//{
		//	return -1;
		//}
		return ret;
	}

	virtual cgc::bigint select(const char * selectSql, int& outCookie)
	{
		if (selectSql == NULL || !isServiceInited()) return -1;
		if (!open()) return -1;

		CMysqlSink * pSink = NULL;
		cgc::bigint rows = 0;
		try
		{
			if (CMysqlPool::IsServerError(m_nLastErrorCode) && (m_tLastErrorTime+10)>time(0))
			{
				return -1;
			}
			pSink = m_mysqlPool.SinkGet(m_nLastErrorCode);
			if (pSink==NULL)
			{
				m_tLastErrorTime = time(0);
				return -1;
			}
			MYSQL * pMysql = pSink->GetMYSQL();
			if(mysql_query(pMysql, selectSql))
			{
				m_tLastErrorTime = time(0);
				m_nLastErrorCode = mysql_errno(pMysql);
				CGC_LOG((cgc::LOG_WARNING, "%s(%d:%s)\n", selectSql,m_nLastErrorCode,mysql_error(pMysql)));
				if (CMysqlPool::IsServerError(m_nLastErrorCode))
				{
					// **重新连接
					//printf("******* Reconnect 222");
					if (!pSink->Reconnect())
					{
						// **重连失败
						CMysqlPool::SinkPut(pSink);
						return -1;
					}
					// **重连成功
					pMysql = pSink->GetMYSQL();
					if(mysql_query(pMysql, selectSql))
					{
						CMysqlPool::SinkPut(pSink);
						return -1;
					}
					// **重新查询成功
				}else
				{
					// **其他错误
					CMysqlPool::SinkPut(pSink);
					return -1;
				}

				//if (nMysqlError==2006 ||	// CR_SERVER_GONE_ERROR
				//	nMysqlError==2013)		// CR_SERVER_LOST
				//{
				//	// **重新连接
				//	if (!pSink->Reconnect())
				//	{
				//		// **重连失败
				//		CMysqlPool::SinkPut(pSink);
				//		return -1;
				//	}
				//	// **重连成功
				//	pMysql = pSink->GetMYSQL();
				//	if(mysql_query(pMysql, selectSql))
				//	{
				//		CMysqlPool::SinkPut(pSink);
				//		return -1;
				//	}
				//	// **重新查询成功
				//}else
				//{
				//	// **其他错误
				//	CMysqlPool::SinkPut(pSink);
				//	return -1;
				//}
			}
			m_nLastErrorCode = 0;
			MYSQL_RES * resultset = mysql_store_result(pMysql);
			rows = (cgc::bigint)mysql_num_rows(resultset);
			if (rows > 0)
			{
				outCookie = (int)resultset;
				m_results.insert(outCookie, CDBC_RESULTSET(pSink,resultset));
			}else
			{
				mysql_free_result(resultset);
				resultset = NULL;
				CMysqlPool::SinkPut(pSink);
			}
		}catch(std::exception&)
		{
			CGC_LOG((cgc::LOG_ERROR, "%s\n", selectSql));
			//printf("******* Reconnect 333");
			if (!pSink->Reconnect())
			{
				CGC_LOG((cgc::LOG_ERROR, "Reconnect error.\n"));
			}
			CMysqlPool::SinkPut(pSink);
			return -1;
		}catch(...)
		{
			CGC_LOG((cgc::LOG_ERROR, "%s\n", selectSql));
			//printf("******* Reconnect 444");
			if (!pSink->Reconnect())
			{
				CGC_LOG((cgc::LOG_ERROR, "Reconnect error.\n"));
			}
			CMysqlPool::SinkPut(pSink);
			return -1;
		}
		return rows;
		//sql::ResultSet * resultset = NULL;
		//sql::Statement * stmt = NULL;
		//try
		//{
		//	stmt = m_con->createStatement();
		//	resultset = stmt->executeQuery(selectSql);
		//	//resultset = stmt->executeQuery("SELECT 'Hello World!' AS _message");
		//}catch (sql::SQLException &e)
		//{
		//	return -1;
		//}

		//if (resultset != NULL)
		//{
		//	if (resultset->rowsCount() > 0)
		//	{
		//		outCookie = (int)stmt;
		//		m_results.insert(outCookie, CDBC_RESULTSET(resultset, stmt));
		//	}else
		//	{
		//		delete resultset;
		//		resultset = NULL;
		//	}
		//}
		//return resultset == NULL ? 0 : (int)resultset->rowsCount();
	}
	int m_nLastErrorCode;
	time_t m_tLastErrorTime;
	virtual cgc::bigint select(const char * selectSql)
	{
		if (selectSql == NULL || !isServiceInited()) return -1;
		if (!open()) return -1;

		CMysqlSink * pSink = NULL;
		cgc::bigint rows = 0;
		try
		{
			if (CMysqlPool::IsServerError(m_nLastErrorCode) && (m_tLastErrorTime+10)>time(0))
			{
				return -1;
			}
			pSink = m_mysqlPool.SinkGet(m_nLastErrorCode);
			if (pSink==NULL)
			{
				m_tLastErrorTime = time(0);
				return -1;
			}
			MYSQL * pMysql = pSink->GetMYSQL();
			if(mysql_query(pMysql, selectSql))
			{
				m_tLastErrorTime = time(0);
				m_nLastErrorCode = mysql_errno(pMysql);
				CGC_LOG((cgc::LOG_WARNING, "%s(%d:%s)\n", selectSql,m_nLastErrorCode,mysql_error(pMysql)));
				if (CMysqlPool::IsServerError(m_nLastErrorCode))
				{
					// **重新连接
					//printf("******* Reconnect 555");
					if (!pSink->Reconnect())
					{
						// **重连失败
						CMysqlPool::SinkPut(pSink);
						return -1;
					}
					// **重连成功
					pMysql = pSink->GetMYSQL();
					if(mysql_query(pMysql, selectSql))
					{
						CMysqlPool::SinkPut(pSink);
						return -1;
					}
					// **重新查询成功
				}else
				{
					// **其他错误
					CMysqlPool::SinkPut(pSink);
					return -1;
				}

				//if (nMysqlError==2006 ||	// CR_SERVER_GONE_ERROR
				//	nMysqlError==2013)		// CR_SERVER_LOST
				//{
				//	// **重新连接
				//	if (!pSink->Reconnect())
				//	{
				//		// **重连失败
				//		CMysqlPool::SinkPut(pSink);
				//		return -1;
				//	}
				//	// **重连成功
				//	pMysql = pSink->GetMYSQL();
				//	if(mysql_query(pMysql, selectSql))
				//	{
				//		CMysqlPool::SinkPut(pSink);
				//		return -1;
				//	}
				//	// **重新查询成功
				//}else
				//{
				//	// **其他错误
				//	CMysqlPool::SinkPut(pSink);
				//	return -1;
				//}
			}
			m_nLastErrorCode = 0;
			MYSQL_RES * resultset = mysql_store_result(pMysql);
			rows = (cgc::bigint)mysql_num_rows(resultset);
			CMysqlPool::SinkPut(pSink);
			mysql_free_result(resultset);
			resultset = NULL;
		}catch(std::exception&)
		{
			CGC_LOG((cgc::LOG_ERROR, "%s\n", selectSql));
			//printf("******* Reconnect 666");
			if (!pSink->Reconnect())
			{
				CGC_LOG((cgc::LOG_ERROR, "Reconnect error.\n"));
			}
			CMysqlPool::SinkPut(pSink);
			return -1;
		}catch(...)
		{
			CGC_LOG((cgc::LOG_ERROR, "%s\n", selectSql));
			//printf("******* Reconnect 777");
			if (!pSink->Reconnect())
			{
				CGC_LOG((cgc::LOG_ERROR, "Reconnect error.\n"));
			}
			CMysqlPool::SinkPut(pSink);
			return -1;
		}
		return rows;
	}
	virtual cgc::bigint size(int cookie) const
	{
		CCDBCResultSet::pointer cdbcResultSet;
		return m_results.find(cookie, cdbcResultSet) ? cdbcResultSet->size() : -1;
	}
	virtual int cols(int cookie) const
	{
		CCDBCResultSet::pointer cdbcResultSet;
		return m_results.find(cookie, cdbcResultSet) ? cdbcResultSet->cols() : -1;
	}

	virtual cgc::bigint index(int cookie) const
	{
		CCDBCResultSet::pointer cdbcResultSet;
		return m_results.find(cookie, cdbcResultSet) ? cdbcResultSet->index() : -1;
	}
	virtual cgcValueInfo::pointer cols_name(int cookie) const
	{
		CCDBCResultSet::pointer cdbcResultSet;
		return m_results.find(cookie, cdbcResultSet) ? cdbcResultSet->cols_name() : cgcNullValueInfo;
	}
	virtual cgcValueInfo::pointer index(int cookie, cgc::bigint moveIndex)
	{
		CCDBCResultSet::pointer cdbcResultSet;
		return m_results.find(cookie, cdbcResultSet) ? cdbcResultSet->index(moveIndex) : cgcNullValueInfo;
	}
	virtual cgcValueInfo::pointer first(int cookie)
	{
		CCDBCResultSet::pointer cdbcResultSet;
		return m_results.find(cookie, cdbcResultSet) ? cdbcResultSet->first() : cgcNullValueInfo;
	}
	virtual cgcValueInfo::pointer next(int cookie)
	{
		CCDBCResultSet::pointer cdbcResultSet;
		return m_results.find(cookie, cdbcResultSet) ? cdbcResultSet->next() : cgcNullValueInfo;
	}
	virtual cgcValueInfo::pointer previous(int cookie)
	{
		CCDBCResultSet::pointer cdbcResultSet;
		return m_results.find(cookie, cdbcResultSet) ? cdbcResultSet->previous() : cgcNullValueInfo;
	}
	virtual cgcValueInfo::pointer last(int cookie)
	{
		CCDBCResultSet::pointer cdbcResultSet;
		return m_results.find(cookie, cdbcResultSet) ? cdbcResultSet->last() : cgcNullValueInfo;
	}
	virtual void reset(int cookie)
	{
		CCDBCResultSet::pointer cdbcResultSet;
		if (m_results.find(cookie, cdbcResultSet, true))
		{
			cdbcResultSet->reset();
		}
	}

	// * new version
	virtual int trans_begin(void)
	{
		if (!isServiceInited()) return 0;
		if (!open()) return 0;

		CMysqlSink* pSink = NULL;
		cgc::bigint ret = 0;
		try
		{
			if (CMysqlPool::IsServerError(m_nLastErrorCode) && (m_tLastErrorTime+10)>time(0))
			{
				return 0;
			}
			pSink = m_mysqlPool.SinkGet(m_nLastErrorCode);
			if (pSink==NULL)
			{
				m_tLastErrorTime = time(0);
				return 0;
			}
			MYSQL * pMysql = pSink->GetMYSQL();
			if (mysql_query(pMysql, "begin")!=0)
			{
				m_tLastErrorTime = time(0);
				m_nLastErrorCode = mysql_errno(pMysql);
				CGC_LOG((cgc::LOG_WARNING, "%d:%s\n", m_nLastErrorCode,mysql_error(pMysql)));
				if (CMysqlPool::IsServerError(m_nLastErrorCode))
				{
					// **重新连接
					if (!pSink->Reconnect())
					{
						// **重连失败
						CMysqlPool::SinkPut(pSink);
						return 0;
					}
					// **重连成功
					pMysql = pSink->GetMYSQL();
					if(mysql_query(pMysql, "begin")!=0)
					{
						CMysqlPool::SinkPut(pSink);
						return 0;
					}
					// **重新查询成功
				}else
				{
					// **其他错误
					CMysqlPool::SinkPut(pSink);
					return 0;
				}
			}
			m_nLastErrorCode = 0;
			return (int)pSink;
		}catch(std::exception&)
		{
			CGC_LOG((cgc::LOG_ERROR, "trans_begin exception\n"));
		}catch(...)
		{
			CGC_LOG((cgc::LOG_ERROR, "trans_begin exception\n"));
		}
		return 0;
	}
	virtual bool trans_rollback(int nTransaction)
	{
		if (!isServiceInited()) return false;
		if (!open()) return false;

		CMysqlSink* pSink = (CMysqlSink*)nTransaction;
		if (pSink==NULL) return false;
		bool result = false;
		try
		{
			MYSQL * pMysql = pSink->GetMYSQL();
			if (mysql_query(pMysql, "rollback")!=0)
			{
				m_tLastErrorTime = time(0);
				m_nLastErrorCode = mysql_errno(pMysql);
				CGC_LOG((cgc::LOG_WARNING, "%d:%s\n", m_nLastErrorCode,mysql_error(pMysql)));
				if (CMysqlPool::IsServerError(m_nLastErrorCode))
				{
					// **重新连接
					if (!pSink->Reconnect())
					{
						// **重连失败
						CMysqlPool::SinkPut(pSink);
						return false;
					}
					// **重连成功
					pMysql = pSink->GetMYSQL();
					if(mysql_query(pMysql, "rollback")!=0)
					{
						CMysqlPool::SinkPut(pSink);
						return false;
					}
					// **重新查询成功
				}else
				{
					// **其他错误
					CMysqlPool::SinkPut(pSink);
					return false;
				}
			}
			m_nLastErrorCode = 0;
			result = true;
		}catch(std::exception&)
		{
			CGC_LOG((cgc::LOG_ERROR, "trans_rollback exception\n"));
		}catch(...)
		{
			CGC_LOG((cgc::LOG_ERROR, "trans_rollback exception\n"));
		}
		CMysqlPool::SinkPut(pSink);
		return result;
	}
	virtual cgc::bigint trans_commit(int nTransaction)
	{
		if (!isServiceInited()) return -1;
		if (!open()) return -1;

		CMysqlSink* pSink = (CMysqlSink*)nTransaction;
		if (pSink==NULL) return -1;
		cgc::bigint result = -1;
		try
		{
			MYSQL * pMysql = pSink->GetMYSQL();
			if (mysql_query(pMysql, "commit")!=0)
			{
				m_tLastErrorTime = time(0);
				m_nLastErrorCode = mysql_errno(pMysql);
				CGC_LOG((cgc::LOG_WARNING, "%d:%s\n", m_nLastErrorCode,mysql_error(pMysql)));
				if (CMysqlPool::IsServerError(m_nLastErrorCode))
				{
					// **重新连接
					if (!pSink->Reconnect())
					{
						// **重连失败
						CMysqlPool::SinkPut(pSink);
						return -1;
					}
					// **重连成功
					pMysql = pSink->GetMYSQL();
					if(mysql_query(pMysql, "commit")!=0)
					{
						CMysqlPool::SinkPut(pSink);
						return -1;
					}
					// **重新查询成功
				}else
				{
					// **其他错误
					CMysqlPool::SinkPut(pSink);
					return -1;
				}
			}
			m_nLastErrorCode = 0;
			result = (cgc::bigint)mysql_affected_rows(pMysql);
			MYSQL_RES * result = 0;
			do
			{
				if ( !(result = mysql_store_result(pMysql)))
				{
					break;
				} 
				mysql_free_result(result);
			}while( !mysql_next_result( pMysql ) );
		}catch(std::exception&)
		{
			CGC_LOG((cgc::LOG_ERROR, "trans_commit exception\n"));
		}catch(...)
		{
			CGC_LOG((cgc::LOG_ERROR, "trans_commit exception\n"));
		}
		CMysqlPool::SinkPut(pSink);
		return result;
	}

	virtual bool auto_commit(bool autocommit)
	{
		if (!isopen()) return false;
		// mysql_query('begin');
		return false;
		//return mysql_autocommit(m_mysql, autocommit ? 1 : 0) == 1;
	}
	virtual bool commit(void)
	{
		if (!isopen()) return false;
		//  mysql_query('commit');
		return false;

		//return mysql_commit(m_mysql) == 1;
	}
	virtual bool rollback(void)
	{
		if (!isopen()) return false;
		//  mysql_query('rollback');
		return false;

		//return mysql_rollback(m_mysql) == 1;
	}
private:
	//sql::Driver * m_driver;
	//sql::Connection * m_con;
	CMysqlPool m_mysqlPool;
	//MYSQL * m_mysql;

	time_t m_tLastTime;
	CLockMap<int, CCDBCResultSet::pointer> m_results;
	cgcCDBCInfo::pointer m_cdbcInfo;
};

const int ATTRIBUTE_NAME = 1;
cgcAttributes::pointer theAppAttributes;
//CMysqlCdbc::pointer theBodbCdbc;
tstring theAppConfPath;

// 模块初始化函数，可选实现函数
extern "C" bool CGC_API CGC_Module_Init(void)
{
	theAppAttributes = theApplication->getAttributes(true);
	assert (theAppAttributes.get() != NULL);

	theAppConfPath = theApplication->getAppConfPath();
	return true;
}

// 模块退出函数，可选实现函数
extern "C" void CGC_API CGC_Module_Free(void)
{
	VoidObjectMapPointer mapLogServices = theAppAttributes->getVoidAttributes(ATTRIBUTE_NAME, false);
	if (mapLogServices.get() != NULL)
	{
		CObjectMap<void*>::iterator iter;
		for (iter=mapLogServices->begin(); iter!=mapLogServices->end(); iter++)
		{
			CMysqlCdbc::pointer service = CGC_OBJECT_CAST<CMysqlCdbc>(iter->second);
			if (service.get() != NULL)
			{
				service->finalService();
			}
		}
	}
	theAppAttributes.reset();
}

extern "C" void CGC_API CGC_GetService(cgcServiceInterface::pointer & outService, const cgcValueInfo::pointer& parameter)
{
	if (theAppAttributes.get() == NULL) return;
	CMysqlCdbc::pointer bodbCdbc = CMysqlCdbc::pointer(new CMysqlCdbc(theAppConfPath));
	outService = bodbCdbc;
	theAppAttributes->setAttribute(ATTRIBUTE_NAME, outService.get(), outService);
}

extern "C" void CGC_API CGC_ResetService(const cgcServiceInterface::pointer & inService)
{
	if (inService.get() == NULL || theAppAttributes.get() == NULL) return;
	theAppAttributes->removeAttribute(ATTRIBUTE_NAME, inService.get());
	inService->finalService();
}

#endif // USES_MYSQLCDBC
