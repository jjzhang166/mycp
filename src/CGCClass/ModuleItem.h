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

// ModuleItem.h file here
#ifndef __ModuleItem_h__
#define __ModuleItem_h__

#include <boost/shared_ptr.hpp>
#include "../ThirdParty/stl/stldef.h"
#include "dlldefine.h"

#include "../CGCBase/cgcdef.h"
#include "MethodItem.h"

namespace cgc
{

typedef	std::map<tstring, tstring> StringMap;
typedef	StringMap::const_iterator StringMapIter;
typedef std::pair<tstring, tstring> StringMapPair;

class CGCCLASS_CLASS ModuleItem
{
public:
	typedef boost::shared_ptr<ModuleItem> pointer;

	static ModuleItem::pointer create(void);
	static ModuleItem::pointer create(int type, const tstring & name, const tstring & module);
	ModuleItem(void);
	ModuleItem(int type, const tstring & name, const tstring & module);
	virtual ~ModuleItem(void);

	enum LockState
	{
		LS_NONE = 0			// "NONE" default
		, LS_WAIT = 1		// "WAIT"
		, LS_RETURN = 2		// "RETURN"
	};

	enum EncryptionType
	{
		ET_MD5 = 1
		, ET_NONE = 0XFF
	};

	static EncryptionType getEncryption(const tstring & sET);
	static tstring getEncryption(EncryptionType nET);

	//
	// MODULETYPE
	static MODULETYPE getModuleType(const tstring & sModuleType);
	static tstring getModuleType(MODULETYPE moduleType);
	//
	// LockState
	static LockState getLockState(const tstring & sLockState);
	static tstring getLockState(LockState lockState);

	StringMap m_mapAllowMethods;
	StringMap m_mapAuths;
public:
	void setName(const tstring & newValue){this->m_name = newValue;}
	const tstring & getName(void) const {return this->m_name;}

	void setModule(const tstring & newValue){this->m_module = newValue;}
	const tstring & getModule(void) const {return this->m_module;}

	void setParam(const tstring& v) {m_param = v;}
	const tstring& getParam(void) const {return m_param;}

	void setType(MODULETYPE newValue){this->m_type = newValue;}
	MODULETYPE getType(void) const {return this->m_type;}
	tstring getTypeString(void) const {return getModuleType(this->m_type);}

	void setProtocol(int newv) {m_protocol = newv;}
	int getProtocol(void) const {return m_protocol;}

	void setAllowAll(bool newValue) {this->m_bAllowAll = newValue;}
	bool getAllowAll(void) const {return this->m_bAllowAll;}
	bool getAllowMethod(const tstring & invokeName, tstring & methodName) const;

	void setAuthAccount(bool newValue) {this->m_bAuthAccount = newValue;}
	bool getAuthAccount(void) const {return this->m_bAuthAccount;}
	bool authAccount(const tstring & account, const tstring & passwd) const;
	
	void setCommPort(int newValue) {m_nCommPort = newValue;}
	int getCommPort(void) const {return m_nCommPort;}

	void setModuleHandle(void * newValue) {this->m_pModuleHandle = newValue;}
	void *getModuleHandle(void) const {return this->m_pModuleHandle;}

	bool isAppModule(void) const {return m_type==MODULE_APP;}
	bool isCommModule(void) const {return m_type==MODULE_COMM;}
	bool isParserModule(void) const {return m_type==MODULE_PARSER;}
	bool isServerModule(void) const {return m_type==MODULE_SERVER;}
	bool isLogModule(void) const {return m_type==MODULE_LOG;}

	//
	// locl state
	void setLockState(LockState newv) {m_lockState = newv;}
	LockState getLockState(void) const {return m_lockState;}

	// disable
	void setDisable(bool newv) {m_bDisable = newv;}
	bool getDisable(void) const {return m_bDisable;}

	virtual void Serialize(bool isStoring, std::fstream& ar);

private:
	tstring m_name;
	tstring m_module;
	tstring m_param;
	MODULETYPE m_type;
	int m_protocol;						// default 0; 0=Protocol_Sotp;1=Protocol_Http
	bool m_bAllowAll;					// for MODULE_APP
	bool m_bAuthAccount;				// for MODULE_APP
	int m_nCommPort;					// for MODULE_COMM, default 0

	void * m_pModuleHandle;
	LockState m_lockState;				// default LS_NONE
	bool m_bDisable;					// default false
};

} // cgc namespace

#endif // __ModuleItem_h__
