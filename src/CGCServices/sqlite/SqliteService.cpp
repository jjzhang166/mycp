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

#define USES_SQLITECDBC		1		// [0,1]

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

#endif // WIN32

#include <CGCBase/cdbc.h>
using namespace cgc;
#include <boost/thread/shared_mutex.hpp>

#if (USES_SQLITECDBC)

#include <sqlite3.h>

#ifdef WIN32
#ifdef _DEBUG
#pragma comment(lib,"sqlite3sd.lib")
#else
#pragma comment(lib,"sqlite3s.lib")
#endif
#endif // WIN32


class CCDBCResultSet
{
public:
	typedef boost::shared_ptr<CCDBCResultSet> pointer;

	cgc::bigint size(void) const {return m_rows;}
	int cols(void) const {return (int)m_fields;}

	cgc::bigint index(void) const
	{
		return m_currentIndex;
	}
	cgcValueInfo::pointer cols_name(void) const
	{
		if (m_resultset == NULL || m_rows == 0 || m_fields==0) return cgcNullValueInfo;
		std::vector<cgcValueInfo::pointer> record;
		try
		{
			const cgc::bigint offset = 0;
			for (int i=0 ; i<m_fields; i++ )
			{
				const tstring s( m_resultset[offset+i]==NULL ? "" : (const char*)m_resultset[offset+i]);
				record.push_back(CGC_VALUEINFO(s));
			}
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
		m_currentIndex = moveIndex;
		return getCurrentRecord();
	}
	cgcValueInfo::pointer first(void)
	{
		if (m_resultset == NULL || m_rows == 0) return cgcNullValueInfo;
		m_currentIndex = 0;
		return getCurrentRecord();
	}
	cgcValueInfo::pointer next(void)
	{
		if (m_resultset == NULL || m_rows == 0) return cgcNullValueInfo;
		if (m_currentIndex+1 == m_rows) return cgcNullValueInfo;
		++m_currentIndex;
		return getCurrentRecord();
	}
	cgcValueInfo::pointer previous(void)
	{
		if (m_resultset == NULL || m_rows == 0) return cgcNullValueInfo;
		if (m_currentIndex == 0) return cgcNullValueInfo;
		--m_currentIndex;
		return getCurrentRecord();
	}
	cgcValueInfo::pointer last(void)
	{
		if (m_resultset == NULL || m_rows == 0) return cgcNullValueInfo;
		m_currentIndex = m_rows - 1;
		return getCurrentRecord();
	}
	void reset(void)
	{
		if (m_resultset != 0)
		{
			try
			{
				sqlite3_free_table( m_resultset );
			}catch(...)
			{}
			m_resultset = NULL;
		}
		m_rows = 0;
	}

	CCDBCResultSet(char** resultset, cgc::bigint rows, short fields)
		: m_resultset(resultset), m_rows(rows), m_fields(fields)
	{
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

		std::vector<cgcValueInfo::pointer> record;
		try
		{
			const cgc::bigint offset = (m_currentIndex+1)*m_fields;	// +1 is column head info
			for (int i=0 ; i<m_fields; i++ )
			{
				const tstring s( m_resultset[offset+i]==NULL ? "" : (const char*)m_resultset[offset+i]);
				record.push_back(CGC_VALUEINFO(s));
			}
		}catch(...)
		{
			// ...
		}

		return CGC_VALUEINFO(record);
	}

private:
	char** m_resultset;
	cgc::bigint	m_rows;
	short		m_fields;
	cgc::bigint	m_currentIndex;
};

#define CDBC_RESULTSET(r,row,field) CCDBCResultSet::pointer(new CCDBCResultSet(r,row,field))

//const int escape_size = 1;
//const std::string escape_in[] = {"''"};
//const std::string escape_out[] = {"'"};

//const int escape_size = 9;
//const std::string escape_in[] = {"//","''","/[","]","/%","/$","/_","/(","/)"};
//const std::string escape_out[] = {"/","'","[","]","%","$","_","(",")"};

const int escape_in_size = 2;
const int escape_out_size = 4;	// ?兼容旧版本
const std::string escape_in[] = {"''","//","&lsquo;","&pge0;"};
const std::string escape_out[] = {"'","/","'","\\"};
//const int escape_size = 2;
//const std::string escape_in[] = {"&lsquo;","&mse0;"};
//const std::string escape_out[] = {"'","\\"};

class CSqliteCdbc
	: public cgcCDBCService
{
public:
	typedef boost::shared_ptr<CSqliteCdbc> pointer;

	CSqliteCdbc(const tstring& path)
		: m_pSqlite(NULL), m_tLastTime(0), m_sAppConfPath(path)
	{}
	virtual ~CSqliteCdbc(void)
	{
		finalService();
	}
	virtual bool initService(cgcValueInfo::pointer parameter)
	{
		if (isServiceInited()) return true;
		m_bServiceInited = true;
		return isServiceInited();
	}
	virtual void finalService(void)
	{
		close();
		m_cdbcInfo.reset();
		m_bServiceInited = false;
	}
private:
	virtual tstring serviceName(void) const {return _T("SQLITECDBC");}

	static const std::string & replace(std::string & strSource, const std::string & strFind, const std::string &strReplace)
	{
		tstring::size_type pos=0;
		tstring::size_type findlen=strFind.size();
		tstring::size_type replacelen=strReplace.size();
		while ((pos=strSource.find(strFind, pos)) != tstring::npos)
		{
			strSource.replace(pos, findlen, strReplace);
			pos += replacelen;
		}
		return strSource;
	}

	virtual void escape_string_in(std::string & str)
	{
		for (int i=0; i<escape_in_size; i++)
			replace(str, escape_out[i], escape_in[i]);
	}
	virtual void escape_string_out(std::string & str)
	{
		for (int i=0; i<escape_out_size; i++)
			replace(str, escape_in[i], escape_out[i]);
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
			//  rc = sqlite3_open(":memory:", &db);
			//  ATTACH DATABASE ':memory:' AS aux1;
			// 
			tstring sDatabase = m_cdbcInfo->getDatabase();
			if (sDatabase!=":memory:")
			{
#ifdef WIN32
				sDatabase = convert(m_sAppConfPath.c_str(), CP_ACP, CP_UTF8);
				sDatabase.append("\\");
#else
				sDatabase = m_sAppConfPath + "/";
#endif
				sDatabase.append(m_cdbcInfo->getDatabase());
			}
			//sqlite3_initialize()
			//sqlite3_config(SQLITE_CONFIG_SERIALIZED)
			const int rc = sqlite3_open(sDatabase.c_str(), &m_pSqlite);
			if ( rc!=SQLITE_OK )  
			{  
				CGC_LOG((cgc::LOG_WARNING, "Can't open database: %s(%s)\n", sDatabase.c_str(),sqlite3_errmsg(m_pSqlite)));
				sqlite3_close(m_pSqlite);
				m_pSqlite = 0;
				return false;  
			}  
		}catch(...)
		{
			return false;
		}

		return true;
	}
	virtual bool open(void) {return open(m_cdbcInfo);}
	virtual void close(void)
	{
		try
		{
			if (m_pSqlite!=0)
			{
				sqlite3_close(m_pSqlite);
				m_pSqlite = 0;
				m_tLastTime = time(0);
			}
		}catch(...)
		{
		}
		m_results.clear();
	}
	virtual bool isopen(void) const
	{
		return (m_pSqlite==0)?false:true;
	}
	virtual time_t lasttime(void) const {return m_tLastTime;}

	//static int sqlite_callback(
	//	void* pv,    /* 由 sqlite3_exec() 的第四个参数传递而来 */
	//	int argc,        /* 表的列数 */
	//	char** argv,    /* 指向查询结果的指针数组, 可以由 sqlite3_column_text() 得到 */
	//	char** col        /* 指向表头名的指针数组, 可以由 sqlite3_column_name() 得到 */
	//	)
	//{
	//	return 0;
	//}
	virtual cgc::bigint execute(const char * exeSql, int nTransaction)
	{
		if (exeSql == NULL || !isServiceInited()) return -1;
		if (!open()) return -1;

		//rc = sqlite3_exec(db, "BEGIN;", 0, 0, &zErrMsg); 
		////执行SQL语句 
		//rc = sqlite3_exec(db, "COMMIT;", 0, 0, &zErrMsg);

		cgc::bigint ret = 0;
		try
		{
			char *zErrMsg = 0;
			boost::mutex::scoped_lock lock(m_mutex);
			int rc = sqlite3_exec( m_pSqlite , exeSql, 0, 0, &zErrMsg);
			if (rc==SQLITE_BUSY)
			{
				sqlite3_free(zErrMsg);
				rc = sqlite3_exec( m_pSqlite , exeSql, 0, 0, &zErrMsg);
			}
			if ( rc!=SQLITE_OK )
			{
				lock.unlock();
				CGC_LOG((cgc::LOG_WARNING, "Can't execute SQL: %d=%s\n", rc,zErrMsg));
				CGC_LOG((cgc::LOG_WARNING, "Can't execute SQL: (%s)\n", exeSql));
				sqlite3_free(zErrMsg);
				//if (nTransaction!=0)
				//	trans_rollback(nTransaction);
				return -1;
			}
			if (nTransaction==0)
				ret = (cgc::bigint)sqlite3_changes(m_pSqlite);
		}catch(...)
		{
			//if (nTransaction!=0)
			//	trans_rollback(nTransaction);
			return -1;
		}
		return ret;
	}

	virtual cgc::bigint select(const char * selectSql, int& outCookie)
	{
		if (selectSql == NULL || !isServiceInited()) return -1;
		if (!open()) return -1;

		cgc::bigint rows = 0;
		try
		{
			int nrow = 0, ncolumn = 0;  
			char *zErrMsg = 0;
			char **azResult = 0; //二维数组存放结果  
			//boost::mutex::scoped_lock lock(m_mutex);
			int rc = sqlite3_get_table( m_pSqlite , selectSql , &azResult , &nrow , &ncolumn , &zErrMsg);
			if (rc==SQLITE_BUSY)
			{
				sqlite3_free(zErrMsg);
				rc = sqlite3_get_table( m_pSqlite , selectSql , &azResult , &nrow , &ncolumn , &zErrMsg);
			}
			if ( rc!=SQLITE_OK )
			{
				CGC_LOG((cgc::LOG_WARNING, "Can't select SQL: (%s); %d=%s\n", selectSql,rc,zErrMsg));
				sqlite3_free(zErrMsg);
				return -1;
			}
			if (azResult != NULL && nrow>0 && ncolumn>0)
			{
				outCookie = (int)azResult;
				m_results.insert(outCookie, CDBC_RESULTSET(azResult, nrow, ncolumn));
				rows = nrow;
			}else
			{
				rows = 0;
				sqlite3_free_table( azResult );
			}
		}catch(...)
		{
			CGC_LOG((cgc::LOG_ERROR, "Select SQL exception: (%s) \n", selectSql));
			return -1;
		}
		return rows;
	}
	virtual cgc::bigint select(const char * selectSql)
	{
		if (selectSql == NULL || !isServiceInited()) return -1;
		if (!open()) return -1;
		cgc::bigint rows = 0;
		try
		{
			int nrow = 0, ncolumn = 0;  
			char *zErrMsg = 0;
			//const int rc = sqlite3_get_table( m_pSqlite , selectSql , 0, &nrow , &ncolumn , &zErrMsg );
			char **azResult = 0; //二维数组存放结果  
			//boost::mutex::scoped_lock lock(m_mutex);
			int rc = sqlite3_get_table( m_pSqlite , selectSql , &azResult , &nrow , &ncolumn , &zErrMsg );
			if (rc==SQLITE_BUSY)
			{
				sqlite3_free(zErrMsg);
				rc = sqlite3_get_table( m_pSqlite , selectSql , &azResult , &nrow , &ncolumn , &zErrMsg );
			}
			if ( rc!=SQLITE_OK )
			{
				CGC_LOG((cgc::LOG_WARNING, "Can't select SQL: (%s); %d=%s\n", selectSql,rc,zErrMsg));
				sqlite3_free(zErrMsg);
				return -1;
			}
			rows = nrow;
			sqlite3_free_table( azResult );
		}catch(...)
		{
			CGC_LOG((cgc::LOG_ERROR, "Select SQL exception: (%s) \n", selectSql));
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
		if (auto_commit(false))
			return 1;
		return 0;
	}
	virtual bool trans_rollback(int nTransaction)
	{
		return rollback();
	}
	virtual cgc::bigint trans_commit(int nTransaction)
	{
		if (!isopen()) return false;
		try
		{
			char *zErrMsg = 0;
			const int rc = sqlite3_exec( m_pSqlite , "COMMIT;", 0, 0, &zErrMsg);
			if ( rc!=SQLITE_OK )
			{
				CGC_LOG((cgc::LOG_WARNING, "Can't COMMIT: %s\n", zErrMsg));
				sqlite3_free(zErrMsg);
				return -1;
			}
			return (cgc::bigint)sqlite3_changes(m_pSqlite);
		}catch(...)
		{
			return -1;
		}
		return -1;
	}

	virtual bool auto_commit(bool autocommit)
	{
		if (!isopen()) return false;
		try
		{
			char *zErrMsg = 0;
			const int rc = sqlite3_exec( m_pSqlite , "BEGIN;", 0, 0, &zErrMsg);
			if ( rc!=SQLITE_OK )
			{
				CGC_LOG((cgc::LOG_WARNING, "Can't BEGIN: %s\n", zErrMsg));
				sqlite3_free(zErrMsg);
				return false;
			}
			return true;
		}catch(...)
		{
			return false;
		}
		return false;
	}
	virtual bool commit(void)
	{
		if (!isopen()) return false;
		try
		{
			char *zErrMsg = 0;
			const int rc = sqlite3_exec( m_pSqlite , "COMMIT;", 0, 0, &zErrMsg);
			if ( rc!=SQLITE_OK )
			{
				CGC_LOG((cgc::LOG_WARNING, "Can't COMMIT: %s\n", zErrMsg));
				sqlite3_free(zErrMsg);
				return false;
			}
			return true;
		}catch(...)
		{
			return false;
		}
		return false;
	}
	virtual bool rollback(void)
	{
		if (!isopen()) return false;
		try
		{
			char *zErrMsg = 0;
			const int rc = sqlite3_exec( m_pSqlite , "ROLLBACK;", 0, 0, &zErrMsg);
			if ( rc!=SQLITE_OK )
			{
				CGC_LOG((cgc::LOG_WARNING, "Can't ROLLBACK: %s\n", zErrMsg));
				sqlite3_free(zErrMsg);
				return false;
			}
			return true;
		}catch(...)
		{
			return false;
		}
		return false;
	}
	
#ifdef WIN32
	static std::string convert(const char * strSource, int sourceCodepage, int targetCodepage)
	{
		int unicodeLen = MultiByteToWideChar(sourceCodepage, 0, strSource, -1, NULL, 0);
		if (unicodeLen <= 0) return "";

		wchar_t * pUnicode = new wchar_t[unicodeLen];
		memset(pUnicode,0,(unicodeLen)*sizeof(wchar_t));

		MultiByteToWideChar(sourceCodepage, 0, strSource, -1, (wchar_t*)pUnicode, unicodeLen);

		char * pTargetData = 0;
		int targetLen = WideCharToMultiByte(targetCodepage, 0, (wchar_t*)pUnicode, -1, (char*)pTargetData, 0, NULL, NULL);
		if (targetLen <= 0)
		{
			delete[] pUnicode;
			return "";
		}

		pTargetData = new char[targetLen];
		memset(pTargetData, 0, targetLen);

		WideCharToMultiByte(targetCodepage, 0, (wchar_t*)pUnicode, -1, (char *)pTargetData, targetLen, NULL, NULL);

		std::string result = pTargetData;
		//	tstring result(pTargetData, targetLen);
		delete[] pUnicode;
		delete[] pTargetData;
		return   result;
	}
#endif
private:
	sqlite3 * m_pSqlite;
	boost::mutex m_mutex;

	time_t m_tLastTime;
	CLockMap<int, CCDBCResultSet::pointer> m_results;
	cgcCDBCInfo::pointer m_cdbcInfo;
	tstring m_sAppConfPath;
};

const int ATTRIBUTE_NAME = 1;
cgcAttributes::pointer theAppAttributes;
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
			CSqliteCdbc::pointer service = CGC_OBJECT_CAST<CSqliteCdbc>(iter->second);
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
	CSqliteCdbc::pointer bodbCdbc = CSqliteCdbc::pointer(new CSqliteCdbc(theAppConfPath));
	outService = bodbCdbc;
	theAppAttributes->setAttribute(ATTRIBUTE_NAME, outService.get(), outService);
}

extern "C" void CGC_API CGC_ResetService(const cgcServiceInterface::pointer & inService)
{
	if (inService.get() == NULL || theAppAttributes.get() == NULL) return;
	theAppAttributes->removeAttribute(ATTRIBUTE_NAME, inService.get());
	inService->finalService();
}

#endif // USES_SQLITECDBC
