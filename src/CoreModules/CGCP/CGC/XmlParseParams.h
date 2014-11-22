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

// XmlParseParsms.h file here
#ifndef __XmlParseParsms_h__
#define __XmlParseParsms_h__
#pragma warning(disable:4819)

#include "IncludeBase.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
using boost::property_tree::ptree;

class XmlParseParams
{
public:
	XmlParseParams(void)
		: m_nMajorVersion(0)
		, m_nMinorVersion(0)
		, m_nMaxSize(1)
		, m_nBackFile(1)
		, m_nLogLevel(20)
		, m_bDtf(true)
		, m_bOlf(true)
		, m_bLts(false)
		, m_sLocale(_T(""))
	{
		m_parameters = cgcParameterMap::pointer(new cgcParameterMap());
	}
	virtual ~XmlParseParams(void)
	{
		m_parameters.reset();
	}

public:
	cgcParameterMap::pointer getParameters(void) const {return m_parameters;}
	void clearParameters(void) {m_parameters->clear();}

	long getMajorVersion(void) const {return m_nMajorVersion;}
	long getMinorVersion(void) const {return m_nMinorVersion;}
	const tstring & getLogPath(void) const {return m_sLogPath;}
	void setLogPath(const tstring & newValue) {m_sLogPath = newValue;}
	const tstring & getLogFile(void) const {return m_sLogFile;}
	void setLogFile(const tstring & newValue) {m_sLogFile = newValue;}
	int getLogMaxSize(void) const {return m_nMaxSize;}	// µ¥Î» 'MB'
	int getLogBackFile(void) const {return m_nBackFile;}
	int getLogLevel(void) const {return m_nLogLevel;}
	bool isDtf(void) const {return m_bDtf;}		// datetime format
	bool isOlf(void) const {return m_bOlf;}		// one line format
	bool isLts(void) const {return m_bLts;}		// log to system
	void setLts(bool newValue) {m_bLts=newValue;}
	const tstring & getLogLocale(void) const {return m_sLocale;}	// locale

	void load(const tstring & filename)
	{
		clearParameters();


		try
		{
			ptree pt;
			read_xml(filename, pt);
			BOOST_FOREACH(const ptree::value_type &v, pt.get_child("root"))
				Insert(v);

		}catch(...)
		{
			int i=0;
		}
	}

private:
	void Insert(const boost::property_tree::ptree::value_type & v)
	{
		if (v.first.compare("parameter") == 0)
		{
			std::string name = v.second.get("name", "");
			std::string type = v.second.get("type", "");
			std::string value = v.second.get("value", "");

			cgcParameter::pointer parameter = CGC_PARAMETER(name, value);
			parameter->totype(cgcValueInfo::cgcGetValueType(type));
			parameter->setAttribute(cgcParameter::ATTRIBUTE_READONLY);
			m_parameters->insert(parameter->getName(), parameter);
		}else if (v.first.compare("log") == 0)
		{
			m_sLogPath = v.second.get("path", "");
			m_sLogFile = v.second.get("file", "");
			m_nMaxSize = v.second.get("maxsize", 1);
			m_nBackFile = v.second.get("backfile", 1);
			//m_nBackFile = v.second.get("logtoserver", 1);
			m_nLogLevel = v.second.get("loglevel", 20);
			m_bDtf = v.second.get("datetimeformat", 1) == 1;
			m_bOlf = v.second.get("onelineformat", 1) == 1;
			m_bLts = v.second.get("logtosystem", 0) == 1;
			m_sLocale = v.second.get("locale", "");
		}else if (v.first.compare("version") == 0)
		{
			m_nMajorVersion = v.second.get("Major", 0);
			m_nMinorVersion = v.second.get("Minor", 0);
		}
	}
private:
	long m_nMajorVersion;
	long m_nMinorVersion;
	tstring m_sLogPath;
	tstring m_sLogFile;
	int m_nMaxSize;		// default 1
	int m_nBackFile;	// default 1
	int m_nLogLevel;	// default 20
	bool m_bDtf;		// default true
	bool m_bOlf;		// default true
	bool m_bLts;		// default false
	tstring m_sLocale;	// default "chs"

	cgcParameterMap::pointer m_parameters;
};

#endif // __XmlParseParsms_h__
