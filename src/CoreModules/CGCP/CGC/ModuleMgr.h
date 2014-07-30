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

// ModuleMgr.h file here
#ifndef _MODULEMGR_HEAD_INCLUDED__
#define _MODULEMGR_HEAD_INCLUDED__

#include "IncludeBase.h"
#include "XmlParseParams.h"
#include <CGCClass/AttributesImpl.h>
//#include "AttributesImpl.h"
#include "TimerInfo.h"

// CModuleImpl
class CModuleImpl
	: public cgcApplication
{
private:
	ModuleItem::pointer m_module;
	cgcAttributes::pointer m_attributes;
	unsigned long m_callRef;					// for lockstate
	TimerTable m_timerTable;			// for timer
	tstring m_sModulePath;
	cgcLogService::pointer m_logService;

	int m_moduleState;	// 0:LoadLibrary; 1:load xml and init()	-1 : free
	bool m_nInited;

	tstring m_sExecuteResult;
public:
	XmlParseParams m_moduleParams;
	std::string m_sTempFile;

public:
	CModuleImpl(void);
	CModuleImpl(const ModuleItem::pointer& module);
	virtual ~CModuleImpl(void);

	void setModulePath(const tstring & modulePath) {m_sModulePath = modulePath;}
	//void setCommId(unsigned long commId) {m_commId = commId;}
	//unsigned long getCommId(void) const {return m_commId;}

	void SetInited(bool nInited) {m_nInited = nInited;}
	void StopModule(void);

	unsigned long getCallRef(void) const {return m_callRef;}
	unsigned long addCallRef(void) {return ++m_callRef;}
	unsigned long releaseCallRef(void) {return m_callRef==0 ? 0 : --m_callRef;}
	boost::mutex m_mutexCallWait;	// for LS_WAIT

	ModuleItem::pointer getModuleItem(void) const {return m_module;}

	void setModuleState(int v) {m_moduleState = v;}
	int getModuleState(void) const {return m_moduleState;}

private:
	// cgcApplication handler
	virtual cgcParameterMap::pointer getInitParameters(void) const {return m_moduleParams.getParameters();}

	virtual unsigned long getApplicationId(void) const {return (unsigned long)m_module.get();}
	virtual tstring getApplicationName(void) const {return m_module.get() == 0 ? _T("") : m_module->getName();}
	virtual tstring getApplicationFile(void) const {return m_module.get() == 0 ? _T("") : m_module->getModule();}
	virtual tstring getApplicationType(void) const {return m_module.get() == 0 ? _T("") : m_module->getTypeString();}
	virtual MODULETYPE getModuleType(void) const {return m_module.get() == 0 ? MODULE_UNKNOWN : m_module->getType();}
	virtual tstring getAppConfPath(void) const;
	virtual bool isInited(void) const {return m_nInited;}
	virtual long getMajorVersion(void) const;
	virtual long getMinorVersion(void) const;

	// timer
	virtual bool SetTimer(unsigned int nIDEvent, unsigned int nElapse, cgcOnTimerHandler::pointer handler, bool bOneShot, const void * pvParam);
	virtual void KillTimer(unsigned int nIDEvent);
	virtual void KillAllTimer(void);

	virtual cgcAttributes::pointer getAttributes(bool create);
	virtual cgcAttributes::pointer getAttributes(void) const {return m_attributes;}
	virtual cgcAttributes::pointer createAttributes(void) const {return cgcAttributes::pointer(new AttributesImpl());}

	virtual tstring getExecuteResult(void) const {return m_sExecuteResult;}
	virtual void setExecuteResult(const tstring & executeResult) {m_sExecuteResult = executeResult;}

public:
	virtual cgcLogService::pointer logService(void) const {return m_logService;}
	virtual void logService(cgcLogService::pointer logService) {m_logService = logService;}
	virtual void log(LogLevel level, const char* format,...);
	virtual void log(LogLevel level, const wchar_t* format,...);
};

// CModuleMgr
class CModuleMgr
{
public:
	CModuleMgr(void);
	virtual ~CModuleMgr(void);

	//CModuleImplMap m_mapModuleImpl;
	CLockMap<void*, cgcApplication::pointer> m_mapModuleImpl;

public:
	void StopModule(void);
	void FreeHandle(void);
};

#endif // _MODULEMGR_HEAD_INCLUDED__
