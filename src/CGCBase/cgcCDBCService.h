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

// cgcCDBCService.h file here
#ifndef __cgccdbcservice_head__
#define __cgccdbcservice_head__

#include <boost/shared_ptr.hpp>
#include "cgccdbcs.h"
#include "cgcService.h"

namespace cgc{

class cgcCDBCService
	: public cgcServiceInterface
{
public:
	typedef boost::shared_ptr<cgcCDBCService> pointer;

	virtual void escape_string_in(std::string & str) = 0;
	virtual void escape_string_out(std::string & str) = 0;

	virtual bool open(const cgcCDBCInfo::pointer& cdbcInfo) = 0;
	virtual bool open(void) = 0;
	virtual void close(void) = 0;
	virtual bool isopen(void) const = 0;
	virtual time_t lasttime(void) const = 0;

	// INSERT, UPDATE, DELETE
	// >=  0 : OK
	// == -1 : ERROR
	virtual int execute(const char * exeSql) = 0;

	// SELECT
	// outCookie > 0 : OK
	virtual int select(const char * selectSql, int& outCookie) = 0;

	// Return ResultSet count
	virtual int size(int cookie) const = 0;

	// Return current index.
	virtual int index(int cookie) const = 0;

	virtual cgcValueInfo::pointer index(int cookie, int moveIndex) = 0;
	virtual cgcValueInfo::pointer first(int cookie) = 0;
	virtual cgcValueInfo::pointer next(int cookie) = 0;
	virtual cgcValueInfo::pointer previous(int cookie) = 0;
	virtual cgcValueInfo::pointer last(int cookie) = 0;
	virtual void reset(int cookie) = 0;

	virtual bool auto_commit(bool autocommit) {return false;}
	virtual bool commit(void) {return false;}
	virtual bool rollback(void) {return false;}

};

const cgcCDBCService::pointer cgcNullCDBCService;

#define CGC_CDBCSERVICE_DEF(s) boost::static_pointer_cast<cgcCDBCService, cgcServiceInterface>(s)

} // cgc namespace

#endif // __cgccdbcservice_head__
