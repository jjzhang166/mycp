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

// DataServiceMgr.h file here
#ifndef _dataservicemgr_h__
#define _dataservicemgr_h__

#include "IncludeBase.h"

class CDataServiceMgr
	: public CLockMap<tstring, cgcCDBCService::pointer>
{
public:
	CDataServiceMgr(void)
	{}
	virtual ~CDataServiceMgr(void)
	{
		clear();
	}

public:
	void clear(void)
	{
		CLockMap<tstring, cgcCDBCService::pointer>::iterator iter;
		for (iter=CLockMap<tstring, cgcCDBCService::pointer>::begin(); iter!=CLockMap<tstring, cgcCDBCService::pointer>::end(); iter++)
		{
			iter->second->close();
			iter->second->finalService();
		}
		CLockMap<tstring, cgcCDBCService::pointer>::clear();
	}
private:

};

#endif // _dataservicemgr_h__
