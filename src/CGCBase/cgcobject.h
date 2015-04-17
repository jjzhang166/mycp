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

// cgcobject.h file here
#ifndef __cgcobject_head__
#define __cgcobject_head__

#include <boost/shared_ptr.hpp>
#include "../ThirdParty/stl/lockmap.h"

namespace cgc{
#ifdef WIN32
	typedef __int64				bigint;
#define cgc_atoi64(a) _atoi64(a)
#else
	typedef long long			bigint;
#define cgc_atoi64(a) atoll(a)
#endif
typedef unsigned char			uint8;
typedef unsigned short			uint16;
typedef unsigned int			uint32;
typedef bigint					uint64;

class cgcObject
{
public:
	typedef boost::shared_ptr<cgcObject> pointer;
};

const cgcObject::pointer cgcNullObject;


template<typename K>
class CObjectMap
	: public CLockMap<K, cgcObject::pointer>
{
public:
	CObjectMap(void)
	{}
	virtual ~CObjectMap(void)
	{
		CLockMap<K, cgcObject::pointer>::clear();
	}
};

typedef boost::shared_ptr<CObjectMap<tstring> >			StringObjectMapPointer;
typedef boost::shared_ptr<CObjectMap<int> >				LongObjectMapPointer;
typedef boost::shared_ptr<CObjectMap<bigint> >			BigIntObjectMapPointer;
typedef boost::shared_ptr<CObjectMap<void*> >			VoidObjectMapPointer;


template<typename N>
class CStringObjectMap
	: public CLockMap<N, StringObjectMapPointer>
{
public:
	CStringObjectMap(void)
	{}
	virtual ~CStringObjectMap(void)
	{
		CLockMap<N, StringObjectMapPointer>::clear();
	}
};

template<typename N>
class CLongObjectMap
	: public CLockMap<N, LongObjectMapPointer>
{
public:
	CLongObjectMap(void)
	{}
	virtual ~CLongObjectMap(void)
	{
		CLockMap<N, LongObjectMapPointer>::clear();
	}
};

template<typename N>
class CBigIntObjectMap
	: public CLockMap<N, BigIntObjectMapPointer>
{
public:
	CBigIntObjectMap(void)
	{}
	virtual ~CBigIntObjectMap(void)
	{
		CLockMap<N, BigIntObjectMapPointer>::clear();
	}
};

template<typename N>
class CVoidObjectMap
	: public CLockMap<N, VoidObjectMapPointer>
{
public:
	CVoidObjectMap(void)
	{}
	virtual ~CVoidObjectMap(void)
	{
		CLockMap<N, VoidObjectMapPointer>::clear();
	}
};

template<typename T> boost::shared_ptr<T> CGC_OBJECT_CAST(boost::shared_ptr<cgcObject> const & r)
{
	return boost::static_pointer_cast<T, cgcObject>(r);
}

template<typename T> boost::shared_ptr<cgcObject> CGC_OBJECT_CAST(boost::shared_ptr<T> const & r)
{
	return boost::static_pointer_cast<cgcObject, T>(r);
}

}

#endif // __cgcobject_head__

