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

// CGCP.cpp : Defines the entry point for the console application.
//
#ifdef WIN32
#endif // WIN32

#include <stdio.h>
#include "iostream"
#include "CGC/CGCApp.h"

#if (USES_NEWVERSION)
typedef int (FAR *FPCGC_app_main)(const char * lpszPath);
#endif

#if defined(WIN32) && !defined(__MINGW_GCC)
#include "SSNTService.h"
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
	setlocale(LC_ALL, ".936");
	bool bService = false;
	if(argc>1)
	{
		for (int i=1; i<argc; i++)
		{
			std::string sParam = argv[i];
			if (sParam == "-S")
			{
				bService = true;
			}
		}
	}

	tstring m_sModulePath;
#ifdef WIN32
	TCHAR chModulePath[MAX_PATH];
	memset(&chModulePath, 0, MAX_PATH);
	::GetModuleFileName(NULL, chModulePath, MAX_PATH);
	TCHAR* temp = (TCHAR*)0;
	temp = _tcsrchr(chModulePath, (unsigned int)'\\');
	chModulePath[temp - chModulePath] = '\0';

	m_sModulePath = chModulePath;
#else
	namespace fs = boost::filesystem;
	fs::path currentPath( fs::initial_path());
	m_sModulePath = currentPath.string();
#endif
#if (USES_NEWVERSION)
	std::string sModuleName = m_sModulePath;
	sModuleName.append("/modules/");
#ifdef WIN32
#ifdef _DEBUG

#if (_MSC_VER == 1200)
	sModuleName.append("MYCore60d.dll");
#elif (_MSC_VER == 1300)
	sModuleName.append("MYCore70d.dll");
#elif (_MSC_VER == 1310)
	sModuleName.append("MYCore71d.dll");
#elif (_MSC_VER == 1400)
	sModuleName.append("MYCore80d.dll");
#elif (_MSC_VER == 1500)
	sModuleName.append("MYCore90d.dll");
#endif

#else // _DEBUG

	#if (_MSC_VER == 1200)
	sModuleName.append("MYCore60.dll");
#elif (_MSC_VER == 1300)
	sModuleName.append("MYCore70.dll");
#elif (_MSC_VER == 1310)
	sModuleName.append("MYCore71.dll");
#elif (_MSC_VER == 1400)
	sModuleName.append("MYCore80.dll");
#elif (_MSC_VER == 1500)
	sModuleName.append("MYCore90.dll");
#endif

#endif // _DEBUG

#else // WIN32
	sModuleName.append("MYCore.so");
#endif // WIN32

	void * hModule = NULL;
#ifdef WIN32
	hModule = LoadLibrary(sModuleName.c_str());
#else
	hModule = dlopen(sModuleName.c_str(), RTLD_LAZY);
#endif
	if (hModule == NULL)
	{
#ifdef WIN32
		printf("Cannot open library: \'%d\'!\n", GetLastError());
#else
		printf("Cannot open library: \'%s\'!\n", dlerror());
#endif
		return 0;
	}

	FPCGC_app_main fp = 0;
#ifdef WIN32
	fp = (FPCGC_app_main)GetProcAddress((HMODULE)hModule, "app_main");
#else
	fp = (FPCGC_app_main)dlsym(hModule, "app_main");
#endif
	if (fp)
		fp(m_sModulePath.c_str());

	//DWORD error = GetLastError();

#ifdef WIN32
	FreeLibrary((HMODULE)hModule);
#else
	dlclose (hModule);
#endif
	return 0;
#else // USES_NEWVERSION

#ifdef _DEBUG
	CGCApp::pointer gApp = CGCApp::create(m_sModulePath);
	gApp->MyMain(bService);
#else // _DEBUG

#if defined(WIN32) && !defined(__MINGW_GCC)
	CSSNTService cService;
    SERVICE_TABLE_ENTRY dispatchTable[] =
    {
		{ (LPTSTR)cService.m_sServiceName.c_str(), (LPSERVICE_MAIN_FUNCTION)cService.ServiceMain},
        { NULL, NULL }
    };

	if((argc>1)&&((*argv[1]=='-')||(*argv[1]=='/')))
	{
		if (argc >= 3)
			cService.m_sServiceName = argv[2];
		if (argc >= 4)
			cService.m_sServiceDisplayName = argv[3];
		if (argc >= 5)
			cService.m_sServiceDescription = argv[4];

		if(_tcsicmp(_T("install"),argv[1]+1)==0)
		{

			std::cout << _T("installing ") << cService.m_sServiceName.c_str() << _T("...") << std::endl;
			if(cService.InstallService())
			{
				std::cout << cService.m_sServiceName.c_str() << _T(" install succeeded!") << std::endl;

				std::cout << _T("==Start ") << cService.m_sServiceName.c_str();
				std::cout << _T(": net start ") << cService.m_sServiceName.c_str() << std::endl;

				std::cout << _T("==Stop ") << cService.m_sServiceName.c_str();
				std::cout << _T(": net stop ") << cService.m_sServiceName.c_str() << std::endl;
			}else
			{
				std::cout << _T("Failed install ") << cService.m_sServiceName.c_str() << std::endl;
			}
		}else if(_tcsicmp(_T("remove"),argv[1]+1)==0)
		{
			std::wcout << _T("removeing ") << cService.m_sServiceName.c_str() << _T("...") << std::endl;
			if(cService.RemoveService())
			{
				std::cout << cService.m_sServiceName.c_str() << _T(" remove succeeded!") << std::endl;
			}else
			{
				std::cout << _T("Failed remove ") << cService.m_sServiceName.c_str() << std::endl;
			}
		}else if(_tcsicmp(_T("run"),argv[1]+1)==0)
		{
			CGCApp::pointer gApp = CGCApp::create(m_sModulePath);
			gApp->MyMain(bService);
		}else
		{
			std::cout << _T("-install install ") << cService.m_sServiceName.c_str() << std::endl;
			std::cout << _T("-remove remove ") << cService.m_sServiceName.c_str() << std::endl;
			std::cout << _T("-run run ") << cService.m_sServiceName.c_str() << std::endl;
		}
	}else
	{
		if(!cService.StartDispatch())
		{
			cService.AddToErrorMessageLog(TEXT("Failed start server��"));

			CGCApp::pointer gApp = CGCApp::create(m_sModulePath);
			gApp->MyMain(bService);
		}
	}
#else
	CGCApp::pointer gApp = CGCApp::create(m_sModulePath);
	gApp->MyMain(bService);
#endif
#endif // _DEBUG
#endif // USES_NEWVERSION
	return 0;
}

