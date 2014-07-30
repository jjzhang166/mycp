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

// XmlParseUpload.h file here
#ifndef __XmlParseUpload_h__
#define __XmlParseUpload_h__
#ifdef WIN32
#pragma warning(disable:4819)
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
using boost::property_tree::ptree;


class XmlParseUpload
{
public:
	XmlParseUpload(void)
		: m_enableUpload(false), m_tempPath("uploads")
		, m_maxFileCount(1), m_maxFileSize(10240), m_maxUploadSize(0)
		, m_enableAllContentType(false)

	{}
	~XmlParseUpload(void)
	{
		clear();
	}

	bool isEnableUpload(void) const {return m_enableUpload;}
	const tstring& getTempPath(void) const {return m_tempPath;}
	int getMaxFileCount(void) const {return m_maxFileCount;}
	int getMaxFileSize(void) const {return m_maxFileSize;}			// KB
	int getMaxUploadSize(void) const {return m_maxUploadSize;}		// MB

	bool isEnableContentType(const tstring& contentType) const
	{
		bool enableOrDisable = m_enableAllContentType;
		m_enableTypes.find(contentType, enableOrDisable);
		return enableOrDisable;
	}
public:
	void clear(void) {m_enableTypes.clear();}

	void load(const tstring & filename)
	{
		ptree pt;
		read_xml(filename, pt);

		m_enableUpload = pt.get("upload.enable-upload", 0) == 1;
		if (!m_enableUpload) return;

		m_tempPath = pt.get("upload.temp-path", "uploads");
		if (m_tempPath.empty())
			m_tempPath = "uploads";
		m_maxFileCount = pt.get("upload.max-file-count", 1);
		m_maxFileSize = pt.get("upload.max-file-size", 10240);
		m_maxUploadSize = pt.get("upload.max-upload-size", 0);
		m_enableAllContentType = pt.get("upload.enable-all-content-type", 0) == 1;

		try
		{
			BOOST_FOREACH(const ptree::value_type &v, pt.get_child("upload.enable-content-type"))
				InsertEnableOrDisable(v, true);

			BOOST_FOREACH(const ptree::value_type &v, pt.get_child("upload.disable-content-type"))
				InsertEnableOrDisable(v,false);

		}catch(...)
		{}
	}

private:
	void InsertEnableOrDisable(const boost::property_tree::ptree::value_type & v, bool enableOrDisable)
	{
		if (m_enableAllContentType && enableOrDisable) return;

		if (v.first == "content-type")
		{
			std::string contentType = v.second.get_value("");
			if (!contentType.empty())
			{
				m_enableTypes.remove(contentType);
				m_enableTypes.insert(contentType, enableOrDisable);
			}
		}
	}

private:
	bool m_enableUpload;
	tstring m_tempPath;
	int m_maxFileCount;
	int m_maxFileSize;
	int m_maxUploadSize;
	bool m_enableAllContentType;
	CLockMap<tstring, bool> m_enableTypes;	// ContentType->(true: enable; false: disable)
};

#endif // __XmlParseUpload_h__
