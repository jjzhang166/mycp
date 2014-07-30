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

// XmlParseDefault.h file here
#ifndef __XmlParseDefault_h__
#define __XmlParseDefault_h__
#pragma warning(disable:4819)

#include "IncludeBase.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>

typedef unsigned short  u_short;

class XmlParseDefault
{
public:
	XmlParseDefault(void)
		: m_sCgcpName(_T(""))
		, m_sCgcpCode(_T(""))
		, m_sCgcpAddress(_T(""))
		, m_nCgcpRank(0)
		, m_nWaitSleep(5)
		, m_sDefaultEncoding(_T("GBK"))
	{}
	~XmlParseDefault(void)
	{}

public:
	void load(const tstring & filename)
	{
		// �����յ�property tree����
		using boost::property_tree::ptree;
		ptree pt;

		// ��XML�ļ����뵽property tree��. �������    // (�޷����ļ�,��������), �׳�һ���쳣.
		read_xml(filename, pt);

		// ���������С����,���ǿ�����get������ָ�������ָ���
		// ���debug.filenameû�ҵ�,�׳�һ���쳣
		//m_sCgcpName = pt.get<std::string>("debug.filename");
		// "root.cgcp.*"
		m_sCgcpName = pt.get("root.cgcp.name", _T("CGCP"));
		m_sCgcpAddress = pt.get("root.cgcp.address", _T("127.0.0.1"));
		m_sCgcpCode = pt.get("root.cgcp.code", _T("cgcp0"));
		m_nCgcpRank = pt.get("root.cgcp.rank", 0);
		// root.time.*
		m_nWaitSleep = pt.get("root.time.waitsleep", 0);
		// root.encoding
		m_sDefaultEncoding = pt.get("root.encoding", _T("GBK"));

		// ����debug.modules�β��������ҵ���ģ�鱣�浽m_modules�� 
		// get_child()����ָ��·�����¼�����.���û���¼����׳��쳣.
		// Property tree����������BidirectionalIterator(˫�������)
		//BOOST_FOREACH(ptree::value_type &v,
		//pt.get_child("debug.modules"))
		//	m_modules.insert(v.second.data());
	}

	const tstring & getCgcpName(void) const {return m_sCgcpName;}
	const tstring & getCgcpAddr(void) const {return m_sCgcpAddress;}
	const tstring & getCgcpCode(void) const {return m_sCgcpCode;}
	int getCgcpRank(void) const {return m_nCgcpRank;}
	int getWaitSleep(void) const {return m_nWaitSleep;}
	const tstring & getDefaultEncoding(void) const {return m_sDefaultEncoding;}

private:
	tstring m_sCgcpName;	// ��������
	tstring m_sCgcpAddress;	// ���ص�ַ
	tstring m_sCgcpCode;	// ���ش���
	int m_nCgcpRank;			// ���ؼ���
	int m_nWaitSleep;			// �ȴ������������
	tstring m_sDefaultEncoding;
};

class CPortApp
{
public:
	typedef boost::shared_ptr<CPortApp> pointer;

	void setPort(int v) {m_port = v;}
	int getPort(void) const {return m_port;}
	void setApp(const tstring & v) {m_app = v;}
	const tstring & getApp(void) const {return m_app;}
	void setFunc(const tstring & v) {m_func = v;}
	const tstring getFunc(void) const {return m_func;}	

	void setModuleHandle(void * v) {m_hModule = v;}
	void * getModuleHandle(void) const {return m_hModule;}
	void setFuncHandle(void * v) {m_hFunc = v;}
	void * getFuncHandle(void) const {return m_hFunc;}

	CPortApp(int port, const tstring& app)
		: m_port(port), m_app(app), m_func("")
		, m_hModule(NULL), m_hFunc(NULL)
	{}
private:
	int m_port;
	tstring m_app;
	tstring m_func;

	void * m_hModule;
	void * m_hFunc;
};

#define PORTAPP_POINTER(p, f) CPortApp::pointer(new CPortApp(p, f))

class XmlParsePortApps
{
public:
	XmlParsePortApps(void)

	{}
	~XmlParsePortApps(void)
	{
		clear();
	}

	CPortApp::pointer getPortApp(int port)
	{
		CPortApp::pointer result;
		m_portApps.find(port, result);
		return result;
	}
	void clear(void) {m_portApps.clear();}
public:
	void load(const tstring & filename)
	{
		// �����յ�property tree����
		using boost::property_tree::ptree;
		ptree pt;

		// ��XML�ļ����뵽property tree��. �������    // (�޷����ļ�,��������), �׳�һ���쳣.
		read_xml(filename, pt);

		try
		{
			BOOST_FOREACH(const ptree::value_type &v, pt.get_child("root"))
				Insert(v);

		}catch(...)
		{}
	}

private:
	void Insert(const boost::property_tree::ptree::value_type & v)
	{
		if (v.first.compare("port-app") == 0)
		{
			int disable = v.second.get("disable", 0);
			if (disable == 1) return;

			int port = v.second.get("port", 0);
			std::string app = v.second.get("app", "");
			if (port == 0 || app.empty()) return;

			std::string func = v.second.get("func", "");

			CPortApp::pointer portApp = PORTAPP_POINTER(port, app);
			portApp->setFunc(func);
			m_portApps.insert(port, portApp);
		}
	}

private:
	CLockMap<int, CPortApp::pointer> m_portApps;

};

#endif // __XmlParseDefault_h__
