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

// cgcvalueinfo.h file here
#ifndef __cgcvalueinfo_head__
#define __cgcvalueinfo_head__

#pragma warning(disable:4996)

#include <vector>
#include <string.h>
#include <boost/shared_ptr.hpp>
#include "../ThirdParty/stl/lockmap.h"
#include "cgcobject.h"

namespace cgc{
	class cgcValueInfo;
	typedef boost::shared_ptr<cgcValueInfo> cgcValueInfoPointer;

	const tstring									cgcEmptyTString	= "";
	const std::vector<cgcValueInfoPointer>			cgcEmptyValueInfoList;
	const CLockMap<tstring, cgcValueInfoPointer>	cgcEmptyValueInfoMap;

	class cgcValueInfo
		: public cgcObject
	{
	public:
		typedef boost::shared_ptr<cgcValueInfo> pointer;
		enum ValueType
		{
			TYPE_INT
			, TYPE_BIGINT
			//, TYPE_TIME
			, TYPE_BOOLEAN
			, TYPE_FLOAT
			, TYPE_POINTER
			, TYPE_STRING
			, TYPE_OBJECT
			, TYPE_VALUEINFO
			, TYPE_VECTOR
			, TYPE_MAP
		};

		enum ValueAttribute
		{
			ATTRIBUTE_READONLY
			, ATTRIBUTE_WRITEONLY
			, ATTRIBUTE_BOTH
		};

		static ValueType cgcGetValueType(const tstring& string);

		ValueType getType(void) const {return m_type;}
		void setAttribute(ValueAttribute v) {m_attribute = v;}
		ValueAttribute getAttribute(void) const {return m_attribute;}

		void setInt(int v) {if (m_attribute != ATTRIBUTE_READONLY) u.m_int = v;}
		int getInt(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? -1 : u.m_int;}

		void setBigInt(bigint v) {if (m_attribute != ATTRIBUTE_READONLY) u.m_bigint = v;}
		bigint getBigInt(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? -1 : u.m_bigint;}

		//void setTime(time_t v) {if (m_attribute != ATTRIBUTE_READONLY) u.m_time = v;}
		//time_t getTime(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? 0 : u.m_time;}

		void setBoolean(bool v) {if (m_attribute != ATTRIBUTE_READONLY) u.m_boolean = v;}
		bool getBoolean(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? false : u.m_boolean;}

		void setFloat(double v) {if (m_attribute != ATTRIBUTE_READONLY) u.m_float = v;}
		double getFloat(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? 0.0 : u.m_float;}

		void setPointer(const void* v) {if (m_attribute != ATTRIBUTE_READONLY) u.m_pointer = v;}
		const void* getPointer(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? NULL : u.m_pointer;}

		void setStr(const tstring& v) {if (m_attribute != ATTRIBUTE_READONLY) m_str = v;}
		const tstring& getStr(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? cgcEmptyTString : m_str;}

		void setObject(const cgcObject::pointer& v) {if (m_attribute != ATTRIBUTE_READONLY) m_object = v;}
		cgcObject::pointer getObject(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? cgcNullObject : m_object;}

		void setValueInfo(const cgcValueInfo::pointer& v) {if (m_attribute != ATTRIBUTE_READONLY) m_valueInfo = v;}
		cgcValueInfo::pointer getValueInfo(void) const {cgcValueInfo::pointer nullValueInfo; return m_attribute == ATTRIBUTE_WRITEONLY ? nullValueInfo : m_valueInfo;}

		void addVector(const cgcValueInfo::pointer& v) {if (v.get() != NULL && m_attribute != ATTRIBUTE_READONLY) m_vector.push_back(v);}
		const std::vector<cgcValueInfo::pointer>& getVector(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? cgcEmptyValueInfoList : m_vector;}

		void insertMap(const tstring & key, const cgcValueInfo::pointer& v) {if (v.get() != NULL && m_attribute != ATTRIBUTE_READONLY) m_map.insert(key, v);}
		void deleteMap(const tstring & key) {if (m_attribute != ATTRIBUTE_READONLY) m_map.remove(key);}
		const CLockMap<tstring, cgcValueInfo::pointer>& getMap(void) const {return m_attribute == ATTRIBUTE_WRITEONLY ? cgcEmptyValueInfoMap : m_map;}

		cgcValueInfo(ValueType type, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(int v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(bigint v, ValueAttribute a = ATTRIBUTE_BOTH);
		//cgcValueInfo(time_t v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(bool v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(double v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(const void* v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(const char* v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(const tstring & v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(const cgcObject::pointer& v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(const cgcValueInfo::pointer& v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(const std::vector<cgcValueInfo::pointer> & v, ValueAttribute a = ATTRIBUTE_BOTH);
		cgcValueInfo(const CLockMap<tstring, cgcValueInfo::pointer> & v, ValueAttribute a = ATTRIBUTE_BOTH);
		virtual ~cgcValueInfo(void);

		bool operator == (const cgcValueInfo::pointer& compare) const;
		bool operator != (const cgcValueInfo::pointer& compare) const;
		bool operator > (const cgcValueInfo::pointer& compare) const;
		bool operator >= (const cgcValueInfo::pointer& compare) const;
		bool operator < (const cgcValueInfo::pointer& compare) const;
		bool operator <= (const cgcValueInfo::pointer& compare) const;
		bool operator && (const cgcValueInfo::pointer& compare) const;
		bool operator || (const cgcValueInfo::pointer& compare) const;

		const cgcValueInfo& operator = (const cgcValueInfo::pointer& v);
		const cgcValueInfo& operator += (const cgcValueInfo::pointer& v);
		const cgcValueInfo& operator -= (const cgcValueInfo::pointer& v);
		const cgcValueInfo& operator *= (const cgcValueInfo::pointer& v);
		const cgcValueInfo& operator /= (const cgcValueInfo::pointer& v);
		const cgcValueInfo& operator %= (const cgcValueInfo::pointer& v);

		tstring toString(void) const;
		tstring typeString(void) const;

		int size(void) const;
		bool empty(void) const;
		cgcValueInfo::pointer copy(void) const;
		void totype(ValueType newtype);
		void reset(void);

	protected:
		ValueType m_type;
		ValueAttribute m_attribute;		// Default ATTRIBUTE_BOTH
		union
		{
			int m_int;
			bigint m_bigint;
			//time_t m_time;
			bool m_boolean;
			double m_float;
			const void* m_pointer;
		}u;
		tstring m_str;
		cgcObject::pointer m_object;
		cgcValueInfo::pointer m_valueInfo;
		std::vector<cgcValueInfo::pointer> m_vector;
		CLockMap<tstring, cgcValueInfo::pointer> m_map;
	};
	const cgcValueInfo::pointer cgcNullValueInfo;

#define CGC_VALUEINFO(v) cgcValueInfo::pointer(new cgcValueInfo(v))
#define CGC_VALUEINFOA(v,a) cgcValueInfo::pointer(new cgcValueInfo(v,a))


	inline cgcValueInfo::cgcValueInfo(ValueType type, ValueAttribute a)
		: m_type(type), m_attribute(a), m_str("")
	{
		memset(&u, 0, sizeof(u));
	}
	inline cgcValueInfo::cgcValueInfo(int v, ValueAttribute a)
		: m_type(TYPE_INT), m_attribute(a), m_str("")
	{
		u.m_int = v;
	}
	inline cgcValueInfo::cgcValueInfo(bigint v, ValueAttribute a)
		: m_type(TYPE_BIGINT), m_attribute(a), m_str("")
	{
		u.m_bigint = v;
	}
	//inline cgcValueInfo::cgcValueInfo(time_t v, ValueAttribute a)
	//	: m_type(TYPE_TIME), m_attribute(a), m_str("")
	//{
	//	u.m_time = v;
	//}
	inline cgcValueInfo::cgcValueInfo(bool v, ValueAttribute a)
		: m_type(TYPE_BOOLEAN), m_attribute(a), m_str("")
	{
		u.m_boolean = v;
	}
	inline cgcValueInfo::cgcValueInfo(double v, ValueAttribute a)
		: m_type(TYPE_FLOAT), m_attribute(a), m_str("")
	{
		u.m_float = v;
	}
	inline cgcValueInfo::cgcValueInfo(const void* v, ValueAttribute a)
		: m_type(TYPE_POINTER), m_attribute(a), m_str("")
	{
		u.m_pointer = v;
	}
	inline cgcValueInfo::cgcValueInfo(const tstring & v, ValueAttribute a)
		: m_type(TYPE_STRING), m_attribute(a), m_str(v)
	{
		memset(&u, 0, sizeof(u));
	}
	inline cgcValueInfo::cgcValueInfo(const char* v, ValueAttribute a)
		: m_type(TYPE_STRING), m_attribute(a), m_str(v)
	{
		memset(&u, 0, sizeof(u));
	}
	inline cgcValueInfo::cgcValueInfo(const cgcObject::pointer& v, ValueAttribute a)
		: m_type(TYPE_OBJECT), m_attribute(a), m_str(""), m_object(v)
	{
		memset(&u, 0, sizeof(u));
	}
	inline cgcValueInfo::cgcValueInfo(const cgcValueInfo::pointer& v, ValueAttribute a)
		: m_type(TYPE_VALUEINFO), m_attribute(a), m_str(""), m_valueInfo(v)
	{
		memset(&u, 0, sizeof(u));
	}
	inline cgcValueInfo::cgcValueInfo(const std::vector<cgcValueInfo::pointer> & v, ValueAttribute a)
		: m_type(TYPE_VECTOR), m_attribute(a), m_str("")
	{
		memset(&u, 0, sizeof(u));
		for (size_t i=0; i<v.size(); i++)
			m_vector.push_back(v[i]);
	}
	inline cgcValueInfo::cgcValueInfo(const CLockMap<tstring, cgcValueInfo::pointer> & v, ValueAttribute a)
		: m_type(TYPE_MAP), m_attribute(a), m_str("")
	{
		memset(&u, 0, sizeof(u));
		CLockMap<tstring, cgcValueInfo::pointer>::const_iterator iter;
		for (iter=v.begin(); iter!=v.end(); iter++)
			m_map.insert(iter->first, iter->second);
	}
	inline cgcValueInfo::~cgcValueInfo(void)
	{
		reset();
	}

	inline cgcValueInfo::ValueType cgcValueInfo::cgcGetValueType(const tstring& string)
	{
		if (string == "int")
		{
			return TYPE_INT;
		}else if (string == "bigint")
		{
			return TYPE_BIGINT;
		//}else if (string == "time")
		//{
		//	return TYPE_TIME;
		}else if (string == "boolean")
		{
			return TYPE_BOOLEAN;
		}else if (string == "float")
		{
			return TYPE_FLOAT;
		}else if (string == "pointer")
		{
			return TYPE_POINTER;
		}else if (string == "string")
		{
			return TYPE_STRING;
		}else if (string == "object")
		{
			return TYPE_OBJECT;
		}else if (string == "valueinfo")
		{
			return TYPE_VALUEINFO;
		}else if (string == "vector")
		{
			return TYPE_VECTOR;
		}
		return TYPE_STRING;
	}

	inline bool cgcValueInfo::operator == (const cgcValueInfo::pointer& compare) const
	{
		if (compare.get() == NULL || m_type != compare->getType()) return false;

		switch (m_type)
		{
		case TYPE_INT:
			return u.m_int == compare->getInt();
		case TYPE_BIGINT:
			return u.m_bigint == compare->getBigInt();
		//case TYPE_TIME:
		//	return u.m_time == compare->getTime();
		case TYPE_BOOLEAN:
			return u.m_boolean == compare->getBoolean();
		case TYPE_FLOAT:
			return u.m_float == compare->getFloat();
		case TYPE_POINTER:
			return u.m_pointer == compare->getPointer();
		case TYPE_STRING:
			return m_str == compare->getStr();
		case TYPE_OBJECT:
			return m_object.get() == compare->getObject().get();
		case TYPE_VALUEINFO:
			{
				if (m_valueInfo.get()  == NULL) return bool(compare.get() == NULL);
				return m_valueInfo->operator ==(compare->getValueInfo());
			}break;
		case TYPE_VECTOR:
			return m_vector.size() == compare->getVector().size();
		case TYPE_MAP:
			return m_map.size() == compare->getMap().size();
		default:
			break;
		}
		return true;
	}
	inline bool cgcValueInfo::operator != (const cgcValueInfo::pointer& compare) const
	{
		if (compare.get() == NULL || m_type != compare->getType()) return true;

		switch (m_type)
		{
		case TYPE_INT:
			return u.m_int != compare->getInt();
		case TYPE_BIGINT:
			return u.m_bigint != compare->getBigInt();
		//case TYPE_TIME:
		//	return u.m_time != compare->getTime();
		case TYPE_BOOLEAN:
			return u.m_boolean != compare->getBoolean();
		case TYPE_FLOAT:
			return u.m_float != compare->getFloat();
		case TYPE_POINTER:
			return u.m_pointer != compare->getPointer();
		case TYPE_STRING:
			return m_str != compare->getStr();
		case TYPE_OBJECT:
			return m_object.get() != compare->getObject().get();
		case TYPE_VALUEINFO:
			{
				if (m_valueInfo.get()  == NULL) return bool(compare.get() != NULL);
				return m_valueInfo->operator !=(compare->getValueInfo());
			}break;
		case TYPE_VECTOR:
			return m_vector.size() != compare->getVector().size();
		case TYPE_MAP:
			return m_map.size() != compare->getMap().size();
		default:
			break;
		}
		return false;
	}
	inline bool cgcValueInfo::operator > (const cgcValueInfo::pointer& compare) const
	{
		if (compare.get() == NULL) return true;
		if (m_type != compare->getType()) return false;

		switch (m_type)
		{
		case TYPE_INT:
			return u.m_int > compare->getInt();
		case TYPE_BIGINT:
			return u.m_bigint > compare->getBigInt();
		//case TYPE_TIME:
		//	return u.m_time > compare->getTime();
		case TYPE_BOOLEAN:
			return u.m_boolean > compare->getBoolean();
		case TYPE_FLOAT:
			return u.m_float > compare->getFloat();
		case TYPE_POINTER:
			return u.m_pointer > compare->getPointer();
		case TYPE_STRING:
			return m_str > compare->getStr();
		case TYPE_OBJECT:
			return m_object.get() > compare->getObject().get();
		case TYPE_VALUEINFO:
			{
				if (m_valueInfo.get()  == NULL) return false;
				return m_valueInfo->operator >(compare->getValueInfo());
			}break;
		case TYPE_VECTOR:
			return m_vector.size() > compare->getVector().size();
		case TYPE_MAP:
			return m_map.size() > compare->getMap().size();
		default:
			break;
		}
		return false;
	}
	inline bool cgcValueInfo::operator >= (const cgcValueInfo::pointer& compare) const
	{
		if (compare.get() == NULL) return true;
		if (m_type != compare->getType()) return false;

		switch (m_type)
		{
		case TYPE_INT:
			return u.m_int >= compare->getInt();
		case TYPE_BIGINT:
			return u.m_bigint >= compare->getBigInt();
		//case TYPE_TIME:
		//	return u.m_time >= compare->getTime();
		case TYPE_BOOLEAN:
			return u.m_boolean >= compare->getBoolean();
		case TYPE_FLOAT:
			return u.m_float >= compare->getFloat();
		case TYPE_POINTER:
			return u.m_pointer >= compare->getPointer();
		case TYPE_STRING:
			return m_str >= compare->getStr();
		case TYPE_OBJECT:
			return m_object.get() >= compare->getObject().get();
		case TYPE_VALUEINFO:
			{
				if (m_valueInfo.get()  == NULL) return compare->getValueInfo().get() == NULL;
				return m_valueInfo->operator >=(compare->getValueInfo());
			}break;
		case TYPE_VECTOR:
			return m_vector.size() >= compare->getVector().size();
		case TYPE_MAP:
			return m_map.size() >= compare->getMap().size();
		default:
			break;
		}
		return false;
	}
	inline bool cgcValueInfo::operator < (const cgcValueInfo::pointer& compare) const
	{
		if (compare.get() == NULL) return false;
		if (m_type != compare->getType()) return false;

		switch (m_type)
		{
		case TYPE_INT:
			return u.m_int < compare->getInt();
		case TYPE_BIGINT:
			return u.m_bigint < compare->getBigInt();
		//case TYPE_TIME:
		//	return u.m_time < compare->getTime();
		case TYPE_BOOLEAN:
			return u.m_boolean < compare->getBoolean();
		case TYPE_FLOAT:
			return u.m_float < compare->getFloat();
		case TYPE_POINTER:
			return u.m_pointer < compare->getPointer();
		case TYPE_STRING:
			return m_str < compare->getStr();
		case TYPE_OBJECT:
			return m_object.get() < compare->getObject().get();
		case TYPE_VALUEINFO:
			{
				if (m_valueInfo.get()  == NULL) return compare->getValueInfo().get() != NULL;
				return m_valueInfo->operator <(compare->getValueInfo());
			}break;
		case TYPE_VECTOR:
			return m_vector.size() < compare->getVector().size();
		case TYPE_MAP:
			return m_map.size() < compare->getMap().size();
		default:
			break;
		}
		return false;
	}
	inline bool cgcValueInfo::operator <= (const cgcValueInfo::pointer& compare) const
	{
		if (compare.get() == NULL) return true;
		if (m_type != compare->getType()) return false;

		switch (m_type)
		{
		case TYPE_INT:
			return u.m_int <= compare->getInt();
		case TYPE_BIGINT:
			return u.m_bigint <= compare->getBigInt();
		//case TYPE_TIME:
		//	return u.m_time <= compare->getTime();
		case TYPE_BOOLEAN:
			return u.m_boolean <= compare->getBoolean();
		case TYPE_FLOAT:
			return u.m_float <= compare->getFloat();
		case TYPE_POINTER:
			return u.m_pointer <= compare->getPointer();
		case TYPE_STRING:
			return m_str <= compare->getStr();
		case TYPE_OBJECT:
			return m_object.get() <= compare->getObject().get();
		case TYPE_VALUEINFO:
			{
				if (m_valueInfo.get()  == NULL) return compare->getValueInfo().get() == NULL;
				return m_valueInfo->operator <=(compare->getValueInfo());
			}break;
		case TYPE_VECTOR:
			return m_vector.size() <= compare->getVector().size();
		case TYPE_MAP:
			return m_map.size() <= compare->getMap().size();
		default:
			break;
		}
		return false;
	}
	inline bool cgcValueInfo::operator && (const cgcValueInfo::pointer& compare) const
	{
		if (compare.get() == NULL) return false;
		if (m_type != compare->getType()) return false;

		switch (m_type)
		{
		case TYPE_INT:
			return u.m_int > 0 && compare->getInt() > 0;
		case TYPE_BIGINT:
			return u.m_bigint > 0 && compare->getBigInt() > 0;
		//case TYPE_TIME:
		//	return u.m_time > 0 && compare->getTime() > 0;
		case TYPE_BOOLEAN:
			return u.m_boolean && compare->getBoolean();
		case TYPE_FLOAT:
			return u.m_float > 0.0 && compare->getFloat() > 0.0;
		case TYPE_POINTER:
			return u.m_pointer != NULL && compare->getPointer() != NULL;
		case TYPE_STRING:
			return !m_str.empty() && !compare->getStr().empty();
		case TYPE_OBJECT:
			return m_object.get() != NULL && compare->getObject().get() != NULL;
		case TYPE_VALUEINFO:
			{
				if (m_valueInfo.get()  == NULL) return false;
				return m_valueInfo->operator &&(compare->getValueInfo());
			}break;
		case TYPE_VECTOR:
			return !m_vector.empty() && !compare->getVector().empty();
		case TYPE_MAP:
			return !m_map.empty() && !compare->getMap().empty();
		default:
			break;
		}
		return false;
	}
	inline bool cgcValueInfo::operator || (const cgcValueInfo::pointer& compare) const
	{
		if (compare.get() == NULL) return false;
		if (m_type != compare->getType()) return false;

		switch (m_type)
		{
		case TYPE_INT:
			return u.m_int > 0 || compare->getInt() > 0;
		case TYPE_BIGINT:
			return u.m_bigint > 0 || compare->getBigInt() > 0;
		//case TYPE_TIME:
		//	return u.m_time > 0 || compare->getTime() > 0;
		case TYPE_BOOLEAN:
			return u.m_boolean || compare->getBoolean();
		case TYPE_FLOAT:
			return u.m_float > 0.0 || compare->getFloat() > 0.0;
		case TYPE_POINTER:
			return u.m_pointer != NULL || compare->getPointer() != NULL;
		case TYPE_STRING:
			return !m_str.empty() || !compare->getStr().empty();
		case TYPE_OBJECT:
			return m_object.get() != NULL || compare->getObject().get() != NULL;
		case TYPE_VALUEINFO:
			{
				if (m_valueInfo.get()  == NULL) return false;
				return m_valueInfo->operator ||(compare->getValueInfo());
			}break;
		case TYPE_VECTOR:
			return !m_vector.empty() || !compare->getVector().empty();
		case TYPE_MAP:
			return !m_map.empty() || !compare->getMap().empty();
		default:
			break;
		}
		return false;
	}
	inline const cgcValueInfo& cgcValueInfo::operator = (const cgcValueInfo::pointer& v)
	{
		if (v.get() != NULL)
		{
			this->reset();
			this->m_type = v->getType();
			switch (m_type)
			{
			case TYPE_INT:
				u.m_int = v->getInt();
				break;
			case TYPE_BIGINT:
				u.m_bigint = v->getBigInt();
				break;
			//case TYPE_TIME:
			//	u.m_time = v->getTime();
			//	break;
			case TYPE_BOOLEAN:
				u.m_boolean = v->getBoolean();
				break;
			case TYPE_FLOAT:
				u.m_float = v->getFloat();
				break;
			case TYPE_POINTER:
				u.m_pointer = v->getPointer();
				break;
			case TYPE_STRING:
				m_str = v->getStr();
				break;
			case TYPE_OBJECT:
				m_object = v->getObject();
				break;
			case TYPE_VALUEINFO:
				break;
			case TYPE_VECTOR:
				{
					for (size_t i=0; i<v->getVector().size(); i++)
					{
						m_vector.push_back(v->getVector()[i]);
					}
				}break;
			case TYPE_MAP:
				{
					CLockMap<tstring, cgcValueInfo::pointer>::const_iterator iter;
					for (iter=v->getMap().begin(); iter!=v->getMap().end(); iter++)
						m_map.insert(iter->first, iter->second);
				}break;
			default:
				break;
			}
		}
		return *this;
	}
	inline const cgcValueInfo& cgcValueInfo::operator += (const cgcValueInfo::pointer& v)
	{
		if (v.get() != NULL)
		{
			switch (m_type)
			{
			case TYPE_INT:
				u.m_int += atoi(v->toString().c_str());
				break;
			case TYPE_BIGINT:
				u.m_bigint += cgc_atoi64(v->toString().c_str());
				break;
//			case TYPE_TIME:
//#ifdef WIN32
//				u.m_time += _atoi64(v->toString().c_str());
//#else
//				u.m_time += atol(v->toString().c_str());
//#endif
				break;
			case TYPE_BOOLEAN:
				u.m_boolean = v->getBoolean() || u.m_boolean;
				break;
			case TYPE_FLOAT:
				u.m_float += atof(v->toString().c_str());
				break;
			case TYPE_POINTER:
				//u.m_pointer += v->getPointer();
				break;
			case TYPE_STRING:
				m_str.append(v->toString());
				break;
			case TYPE_OBJECT:
				//m_object = v->getObject();
				break;
			case TYPE_VALUEINFO:
				if (m_valueInfo.get() != NULL)
					m_valueInfo->operator +=(v);
				break;
			case TYPE_VECTOR:
				{
					bool doPush = true;
					for (size_t i=0; i<m_vector.size(); i++)
					{
						if (m_vector[i].get() == v.get())
						{
							m_vector[i]->operator +=(v);
							doPush = false;
							break;
						}
					}
					if (doPush)
						m_vector.push_back(v->copy());
				}break;
			case TYPE_MAP:
				{
					CLockMap<tstring, cgcValueInfo::pointer>::const_iterator iter;
					for (iter=m_map.begin(); iter!=m_map.end(); iter++)
					{
						if (iter->second.get() == v.get())
							iter->second->operator +=(v);
					}
				}break;
			default:
				break;
			}
		}
		return *this;
	}
	inline const cgcValueInfo& cgcValueInfo::operator -= (const cgcValueInfo::pointer& v)
	{
		if (v.get() != NULL)
		{
			switch (m_type)
			{
			case TYPE_INT:
				u.m_int -= atoi(v->toString().c_str());
				break;
			case TYPE_BIGINT:
				u.m_bigint -= cgc_atoi64(v->toString().c_str());
				break;
//			case TYPE_TIME:
//#ifdef WIN32
//				u.m_time -= _atoi64(v->toString().c_str());
//#else
//				u.m_time -= atoll(v->toString().c_str());
//#endif
//				break;
			case TYPE_BOOLEAN:
				u.m_boolean = v->getBoolean() == u.m_boolean;
				break;
			case TYPE_FLOAT:
				u.m_float -= atof(v->toString().c_str());
				break;
			case TYPE_POINTER:
				//u.m_pointer -= v->getPointer();
				break;
			case TYPE_STRING:
				m_str.append(v->toString());
				break;
			case TYPE_OBJECT:
				//m_object = v->getObject();
				break;
			case TYPE_VALUEINFO:
				if (m_valueInfo.get() != NULL)
					m_valueInfo->operator -=(v);
				break;
			case TYPE_VECTOR:
				{
					for (size_t i=0; i<m_vector.size(); i++)
					{
						if (m_vector[i].get() == v.get())
						{
							m_vector[i]->operator -=(v);
							break;
						}
					}
				}break;
			case TYPE_MAP:
				{
					CLockMap<tstring, cgcValueInfo::pointer>::const_iterator iter;
					for (iter=m_map.begin(); iter!=m_map.end(); iter++)
					{
						if (iter->second.get() == v.get())
							iter->second->operator -=(v);
					}
				}break;
			default:
				break;
			}
		}
		return *this;
	}
	inline const cgcValueInfo& cgcValueInfo::operator *= (const cgcValueInfo::pointer& v)
	{
		if (v.get() != NULL)
		{
			switch (m_type)
			{
			case TYPE_INT:
				u.m_int *= atoi(v->toString().c_str());
				break;
			case TYPE_BIGINT:
				u.m_bigint *= cgc_atoi64(v->toString().c_str());
				break;
//			case TYPE_TIME:
//#ifdef WIN32
//				u.m_time *= _atoi64(v->toString().c_str());
//#else
//				u.m_time *= atoll(v->toString().c_str());
//#endif
//				break;
			case TYPE_BOOLEAN:
				u.m_boolean = v->getBoolean() && u.m_boolean;
				break;
			case TYPE_FLOAT:
				u.m_float *= atof(v->toString().c_str());
				break;
			case TYPE_POINTER:
				//u.m_pointer *= v->getPointer();
				break;
			case TYPE_STRING:
				m_str.append(v->toString());
				break;
			case TYPE_OBJECT:
				//m_object = v->getObject();
				break;
			case TYPE_VALUEINFO:
				if (m_valueInfo.get() != NULL)
					m_valueInfo->operator *=(v);
				break;
			case TYPE_VECTOR:
				{
					bool doPush = true;
					for (size_t i=0; i<m_vector.size(); i++)
					{
						if (m_vector[i].get() == v.get())
						{
							m_vector[i]->operator *=(v);
							doPush = false;
							break;
						}
					}
					if (doPush)
						m_vector.push_back(v->copy());
				}break;
			case TYPE_MAP:
				{
					CLockMap<tstring, cgcValueInfo::pointer>::const_iterator iter;
					for (iter=m_map.begin(); iter!=m_map.end(); iter++)
					{
						if (iter->second.get() == v.get())
							iter->second->operator *=(v);
					}
				}break;
			default:
				break;
			}
		}
		return *this;
	}
	inline const cgcValueInfo& cgcValueInfo::operator /= (const cgcValueInfo::pointer& v)
	{
		if (v.get() != NULL)
		{
			switch (m_type)
			{
			case TYPE_INT:
				u.m_int /= atoi(v->toString().c_str());
				break;
			case TYPE_BIGINT:
				u.m_bigint /= cgc_atoi64(v->toString().c_str());
				break;
//			case TYPE_TIME:
//#ifdef WIN32
//				u.m_time /= _atoi64(v->toString().c_str());
//#else
//				u.m_time /= atoll(v->toString().c_str());
//#endif
//				break;
			case TYPE_BOOLEAN:
				u.m_boolean = v->getBoolean() && u.m_boolean;
				break;
			case TYPE_FLOAT:
				u.m_float /= atof(v->toString().c_str());
				break;
			case TYPE_POINTER:
				//u.m_pointer *= v->getPointer();
				break;
			case TYPE_STRING:
				//m_str.append(v->toString());
				break;
			case TYPE_OBJECT:
				//m_object = v->getObject();
				break;
			case TYPE_VALUEINFO:
				if (m_valueInfo.get() != NULL)
					m_valueInfo->operator /=(v);
				break;
			case TYPE_VECTOR:
				{
					for (size_t i=0; i<m_vector.size(); i++)
					{
						if (m_vector[i].get() == v.get())
						{
							m_vector[i]->operator /=(v);
							break;
						}
					}
				}break;
			case TYPE_MAP:
				{
					CLockMap<tstring, cgcValueInfo::pointer>::const_iterator iter;
					for (iter=m_map.begin(); iter!=m_map.end(); iter++)
					{
						if (iter->second.get() == v.get())
							iter->second->operator /=(v);
					}
				}break;
			default:
				break;
			}
		}
		return *this;
	}
	inline const cgcValueInfo& cgcValueInfo::operator %= (const cgcValueInfo::pointer& v)
	{
		if (v.get() != NULL)
		{
			switch (m_type)
			{
			case TYPE_INT:
				u.m_int %= atoi(v->toString().c_str());
				break;
			case TYPE_BIGINT:
				u.m_bigint %= cgc_atoi64(v->toString().c_str());
				break;
//			case TYPE_TIME:
//#ifdef WIN32
//				u.m_time %= _atoi64(v->toString().c_str());
//#else
//				u.m_time %= atoll(v->toString().c_str());
//#endif
//				break;
			case TYPE_BOOLEAN:
				//u.m_boolean = v->getBoolean() && u.m_boolean;
				break;
			case TYPE_FLOAT:
				//((long)u.m_float) %= atol(v->toString().c_str());
				break;
			case TYPE_POINTER:
				//u.m_pointer *= v->getPointer();
				break;
			case TYPE_STRING:
				//m_str.append(v->toString());
				break;
			case TYPE_OBJECT:
				//m_object = v->getObject();
				break;
			case TYPE_VALUEINFO:
				if (m_valueInfo.get() != NULL)
					m_valueInfo->operator %=(v);
				break;
			case TYPE_VECTOR:
				{
					for (size_t i=0; i<m_vector.size(); i++)
					{
						if (m_vector[i].get() == v.get())
						{
							m_vector[i]->operator %=(v);
							break;
						}
					}
				}break;
			case TYPE_MAP:
				{
					CLockMap<tstring, cgcValueInfo::pointer>::const_iterator iter;
					for (iter=m_map.begin(); iter!=m_map.end(); iter++)
					{
						if (iter->second.get() == v.get())
							iter->second->operator %=(v);
					}
				}break;
			default:
				break;
			}
		}
		return *this;
	}
	inline std::string cgcValueInfo::toString(void) const
	{
		switch (m_type)
		{
		case TYPE_INT:
			{
				char buffer[12];
				sprintf(buffer, "%d", u.m_int);
				return std::string(buffer);
			}
		case TYPE_BIGINT:
			{
				char buffer[32];
#ifdef WIN32
				sprintf(buffer, "%I64d", u.m_bigint);
#else
				sprintf(buffer, "%lld", u.m_bigint);
#endif
				return std::string(buffer);
			}
//		case TYPE_TIME:
//			{
//				char buffer[32];
//#ifdef WIN32
//				sprintf(buffer, "%I64d", u.m_time);
//#else
//				sprintf(buffer, "%ld", u.m_time);
//#endif
//				return std::string(buffer);
//			}
		case TYPE_BOOLEAN:
			return u.m_boolean ? "true" : "false";
		case TYPE_FLOAT:
			{
				char buffer[30];
				sprintf(buffer, "%f", u.m_float);
				return std::string(buffer);
			}
		case TYPE_POINTER:
			{
				char buffer[32];
				sprintf(buffer, "pointer:%p", u.m_pointer);
				return std::string(buffer);
			}
		case TYPE_STRING:
			return m_str;
		case TYPE_OBJECT:
			{
				char buffer[32];
				sprintf(buffer, "object:%p", m_object.get());
				return std::string(buffer);
			}
		case TYPE_VALUEINFO:
			return (m_valueInfo.get()  == NULL) ? std::string("") :  m_valueInfo->toString();
		case TYPE_VECTOR:
			{
				std::string result = "";
				for (size_t i=0; i<m_vector.size(); i++)
				{
					if (!result.empty())
						result.append(",");
					result.append(m_vector[i]->toString());
				}
				return result;
			}break;
		case TYPE_MAP:
			{
				std::string result = "";
				CLockMap<tstring, cgcValueInfo::pointer>::const_iterator iter;
				for (iter=m_map.begin(); iter!=m_map.end(); iter++)
				{
					if (!result.empty())
						result.append(",");
					result.append(iter->first);
					result.append("=");
					result.append(iter->second->toString());
				}
				return result;
			}break;
		default:
			break;
		}

		return std::string("");
	}
	inline tstring cgcValueInfo::typeString(void) const
	{
		switch (m_type)
		{
		case TYPE_INT:
			return "int";
		case TYPE_BIGINT:
			return "bigint";
		//case TYPE_TIME:
		//	return "time";
		case TYPE_BOOLEAN:
			return "boolean";
		case TYPE_FLOAT:
			return "float";
		case TYPE_POINTER:
			return "pointer";
		case TYPE_STRING:
			return "string";
		case TYPE_OBJECT:
			return "object";
		case TYPE_VALUEINFO:
			return "valueinfo";
		case TYPE_VECTOR:
			return "vector";
		case TYPE_MAP:
			return "map";
		default:
			break;
		}
		return "unknown";
	}
	inline int cgcValueInfo::size(void) const
	{
		switch (m_type)
		{
		case TYPE_INT:
		case TYPE_BIGINT:
		//case TYPE_TIME:
		case TYPE_STRING:
		case TYPE_BOOLEAN:
		case TYPE_FLOAT:
			return 1;
		case TYPE_POINTER:
			return u.m_pointer == NULL ? 0 : 1;
		case TYPE_OBJECT:
			return m_object.get() == NULL ? 0 : 1;
		case TYPE_VALUEINFO:
			return (m_valueInfo.get()  == NULL) ? -1 :  m_valueInfo->size();
		case TYPE_VECTOR:
			return (int)m_vector.size();
		case TYPE_MAP:
			return (int)m_map.size();
		default:
			break;
		}
		return -1;
	}
	inline bool cgcValueInfo::empty(void) const
	{
		switch (m_type)
		{
		case TYPE_INT:
		case TYPE_BIGINT:
		//case TYPE_TIME:
		case TYPE_BOOLEAN:
		case TYPE_FLOAT:
			return false;
		case TYPE_STRING:
			return m_str.empty();
		case TYPE_POINTER:
			return u.m_pointer == NULL;
		case TYPE_OBJECT:
			return m_object.get() == NULL;
		case TYPE_VALUEINFO:
			return (m_valueInfo.get()  == NULL) ? true :  m_valueInfo->empty();
		case TYPE_VECTOR:
			return m_vector.empty();
		case TYPE_MAP:
			return m_map.empty();
		default:
			break;
		}
		return true;
	}

	inline cgcValueInfo::pointer cgcValueInfo::copy(void) const
	{
		cgcValueInfo::pointer result;
		switch (m_type)
		{
		case TYPE_INT:
			result = cgcValueInfo::pointer(new cgcValueInfo(u.m_int));
			break;
		case TYPE_BIGINT:
			result = cgcValueInfo::pointer(new cgcValueInfo(TYPE_BIGINT));
			result->setBigInt(u.m_bigint);
			break;
		//case TYPE_TIME:
		//	result = cgcValueInfo::pointer(new cgcValueInfo(u.m_time));
		//	break;
		case TYPE_BOOLEAN:
			result = cgcValueInfo::pointer(new cgcValueInfo(u.m_boolean));
			break;
		case TYPE_FLOAT:
			result = cgcValueInfo::pointer(new cgcValueInfo(u.m_float));
			break;
		case TYPE_POINTER:
			result = cgcValueInfo::pointer(new cgcValueInfo(u.m_pointer));
			break;
		case TYPE_STRING:
			result = cgcValueInfo::pointer(new cgcValueInfo(m_str));
			break;
		case TYPE_OBJECT:
			result = cgcValueInfo::pointer(new cgcValueInfo(m_object));
			break;
		case TYPE_VALUEINFO:
			result = cgcValueInfo::pointer(new cgcValueInfo(m_valueInfo));
			break;
		case TYPE_VECTOR:
			result = cgcValueInfo::pointer(new cgcValueInfo(m_vector));
			break;
		case TYPE_MAP:
			result = CGC_VALUEINFO(m_map);
			break;
		default:
			break;
		}
		return result;
	}

	inline void cgcValueInfo::totype(ValueType newtype)
	{
		if (m_attribute != ATTRIBUTE_READONLY && m_type != newtype)
		{
			switch (newtype)
			{
			case TYPE_INT:
				{
					tstring newString = this->toString();
					u.m_int = atoi(newString.c_str());
				}break;
			case TYPE_BIGINT:
				{
					// ??
					const tstring newString = this->toString();
					u.m_bigint = cgc_atoi64(newString.c_str());
				}break;
//			case TYPE_TIME:
//				{
//					// ??
//					tstring newString = this->toString();
//#ifdef WIN32
//					u.m_time = (time_t)_atoi64(newString.c_str());
//#else
//					u.m_time = (time_t)atoll(newString.c_str());
//#endif
//				}break;
			case TYPE_BOOLEAN:
				{
					tstring newString = this->toString();
					u.m_boolean = (newString == "false" || newString == "0") ? false : !newString.empty();
				}break;
			case TYPE_FLOAT:
				{
					tstring newString = this->toString();
					u.m_float = atof(newString.c_str());
				}break;
			case TYPE_POINTER:
				u.m_pointer = NULL;
				break;
			case TYPE_STRING:
				m_str = toString();
				break;
			case TYPE_OBJECT:
				m_object.reset();
				break;
			case TYPE_VALUEINFO:
				{
					if(m_valueInfo.get() != NULL)
						m_valueInfo->totype(newtype);
					else
					{
						tstring newString = this->toString();
						m_valueInfo = cgcValueInfo::pointer(new cgcValueInfo(newString));
					}
				}break;
			case TYPE_VECTOR:
				{
					if (m_vector.empty())
					{
						tstring newString = this->toString();
						m_vector.push_back(cgcValueInfo::pointer(new cgcValueInfo(newString)));
					}else
					{
						// ??
						//for (size_t i=0; i<m_vector.size(); i++)
						//{
						//	m_vector[i]->totype(newtype);
						//}
					}
				}break;
			case TYPE_MAP:
				{
					if (m_vector.empty())
					{
						tstring newString = this->toString();
						m_map.insert("0", CGC_VALUEINFO(newString));
					}else
					{
						// ??
						//for (size_t i=0; i<m_vector.size(); i++)
						//{
						//	m_vector[i]->totype(newtype);
						//}
					}
				}break;
			default:
				break;
			}
			m_type = newtype;
		}
	}

	inline void cgcValueInfo::reset(void)
	{
		memset(&u, 0, sizeof(u));
		m_str.clear();
		m_object.reset();
		m_valueInfo.reset();
		m_vector.clear();
		m_map.clear();
		//m_type = TYPE_STRING;
	}
}

#endif // __cgcvalueinfo_head__
