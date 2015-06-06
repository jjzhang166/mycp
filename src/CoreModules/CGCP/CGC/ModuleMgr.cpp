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

#ifdef WIN32
#include <Windows.h>
#endif

#include "ModuleMgr.h"
#include <algorithm>
#include <stdarg.h>
using namespace std;

CModuleImpl::CModuleImpl(void)
: m_callRef(0)
, m_moduleState(0)
, m_nInited(false)
{
}

CModuleImpl::CModuleImpl(const ModuleItem::pointer& module)
: m_module(module)
, m_callRef(0)
, m_moduleState(0)
, m_nInited(false)

{
}

CModuleImpl::~CModuleImpl(void)
{
	StopModule();
}

void CModuleImpl::StopModule(void)
{
	m_timerTable.KillAll();
	m_logService.reset();
	m_moduleParams.clearParameters();
	m_attributes.reset();
}

tstring CModuleImpl::getAppConfPath(void) const
{
	tstring result(m_sModulePath);
	result.append(_T("/conf/"));
	result.append(getApplicationName());

	return result;
}

long CModuleImpl::getMajorVersion(void) const
{
	return m_moduleParams.getMajorVersion();
}

long CModuleImpl::getMinorVersion(void) const
{
	return m_moduleParams.getMinorVersion();
}

bool CModuleImpl::SetTimer(unsigned int nIDEvent, unsigned int nElapse, cgcOnTimerHandler::pointer handler, bool bOneShot, const void * pvParam)
{
	return m_timerTable.SetTimer(nIDEvent, nElapse, handler, bOneShot, pvParam);
}

void CModuleImpl::KillTimer(unsigned int nIDEvent)
{
	return m_timerTable.KillTimer(nIDEvent);
}

void CModuleImpl::KillAllTimer(void)
{
	return m_timerTable.KillAll();
}

cgcAttributes::pointer CModuleImpl::getAttributes(bool create)
{
	if (create && m_attributes.get() == NULL)
		m_attributes = cgcAttributes::pointer(new AttributesImpl());
	return m_attributes;
}

void CModuleImpl::log(LogLevel level, const char* format,...)
{
	if (m_logService.get() != NULL)
	{
		if (!m_logService->isLogLevel(level))
			return;
	}else if (!m_moduleParams.isLts())
	//}else if ((m_moduleParams.getLogLevel()&(int)level)==0)
	{
		return;
	}
	char debugmsg[MAX_LOG_SIZE];
	memset(debugmsg, 0, MAX_LOG_SIZE);
	va_list   vl;
	va_start(vl, format);
	int len = vsnprintf(debugmsg, MAX_LOG_SIZE-1, format, vl);
	va_end(vl);
	if (len > MAX_LOG_SIZE)
		len = MAX_LOG_SIZE;
	else if (len<=0)
		return;
	debugmsg[len] = '\0';

	if (m_logService.get() != NULL)
	{
		m_logService->log2(level, debugmsg);
	}else if (m_moduleParams.isLts())
	{
		std::cerr << debugmsg;
	}
}

void CModuleImpl::log(LogLevel level, const wchar_t* format,...)
{
	if (m_logService.get() != NULL)
	{
		if (!m_logService->isLogLevel(level))
			return;
	}else if (!m_moduleParams.isLts())
	//}else if ((m_moduleParams.getLogLevel()&(int)level)==0)
	{
		return;
	}
	wchar_t debugmsg[MAX_LOG_SIZE];
	memset(debugmsg, 0, MAX_LOG_SIZE);
	va_list   vl;
	va_start(vl, format);
	int len = vswprintf(debugmsg, MAX_LOG_SIZE-1, format, vl);
	va_end(vl);
	if (len > MAX_LOG_SIZE)
		len = MAX_LOG_SIZE;
	debugmsg[len] = L'\0';

	if (m_logService.get() != NULL)
	{
		m_logService->log2(level, debugmsg);
	}else if (m_moduleParams.isLts())
	{
		std::wcerr<< debugmsg;
	}
}

// CModuleMgr
CModuleMgr::CModuleMgr(void)
{
}

CModuleMgr::~CModuleMgr(void)
{
	FreeHandle();
}

void CModuleMgr::StopModule(void)
{
	BoostReadLock rdlock(m_mapModuleImpl.mutex());
	//for_each(m_mapModuleImpl.begin(), m_mapModuleImpl.end(),
	//	boost::bind(&CModuleMgr::stopModule, boost::bind(&std::map<unsigned long, cgcApplication::pointer>::value_type::second,_1)));

	CLockMap<void*, cgcApplication::pointer>::iterator pIter;
	for (pIter=m_mapModuleImpl.begin(); pIter!=m_mapModuleImpl.end(); pIter++)
	{
		CModuleImpl * pModuleImpl = (CModuleImpl*)pIter->second.get();
		pModuleImpl->StopModule();
	}
}

void CModuleMgr::FreeHandle(void)
{
	StopModule();
	m_mapModuleImpl.clear();
}


