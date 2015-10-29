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
	const std::string sProgram(argv[0]);
	bool bService = false;
	bool bProtect = false;
	int nWaitSeconds=0;
	if (argc>1)
	{
		for (int i=1; i<argc; i++)
		{
			const std::string sParam(argv[i]);
			if (sParam == "-S")
			{
				bService = true;
			}else if (sParam == "-P")
			{
				bProtect = true;
			}else if (sParam == "-W" && (i+1)<argc)
			{
				nWaitSeconds = atoi(argv[i+1]);
				//printf("******* nWaitSeconds = %d\n",nWaitSeconds);
				i++;
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
	//printf("** program=%s,%d\n",sProgram.c_str(),(int)(bService?1:0));
	const std::string sProtectDataFile = m_sModulePath + "/CGCP.protect";
	char lpszBuffer[260];
	if (bProtect)
	{
		const time_t tStartTime = time(0);
		int nErrorCount = 0;
		while (true)
		{
#ifdef WIN32
			Sleep(1000);
#else
			sleep(1);
#endif
			const time_t tCurrentTime = time(0);
			if (tStartTime+10>tCurrentTime)
				continue;
			FILE * pfile = fopen(sProtectDataFile.c_str(),"r");
			if (pfile==NULL)
			{
				if ((nErrorCount++)>=3)
				{
					// 3 second protect file not exist, exit protect.
					break;
				}
			}else
			{
				nErrorCount = 0;
				memset(lpszBuffer,0,sizeof(lpszBuffer));
				fread(lpszBuffer,1,sizeof(lpszBuffer),pfile);
				fclose(pfile);
				const time_t tRunTime = cgc_atoi64(lpszBuffer);
				if (tRunTime>0 && (tCurrentTime-tRunTime)>=8)
				{
					const std::string sProtectLogFile = m_sModulePath + "/CGCP.protect.log";
					FILE * pProtectLog = fopen(sProtectLogFile.c_str(),"a");
					if (pProtectLog!=NULL)
					{
						const struct tm *newtime = localtime(&tRunTime);
						char lpszDateDir[64];
						sprintf(lpszDateDir,"%04d-%02d-%02d %02d:%02d:%02d\n",newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec);
						fwrite(lpszDateDir,1,strlen(lpszDateDir),pProtectLog);
						fclose(pProtectLog);
					}
#ifdef WIN32
					if (strstr(lpszBuffer,",1")!=NULL)
						ShellExecute(NULL,"open",sProgram.c_str(),"-run -S -W 6",m_sModulePath.c_str(),SW_SHOW);
					else
						ShellExecute(NULL,"open",sProgram.c_str(),"-run -W 6",m_sModulePath.c_str(),SW_SHOW);
#else
					sprintf(lpszBuffer,"\"%s\" -S -W 6 &",sProgram.c_str());
					system(lpszBuffer);
#endif
					break;
				}
			}
		}
		return 0;
	}else
	{
		FILE * pfile = fopen(sProtectDataFile.c_str(),"w");
		if (pfile!=NULL)
		{
			sprintf(lpszBuffer,"%lld,%d",(cgc::bigint)time(0),(int)(bService?1:0));
			fwrite(lpszBuffer,1,strlen(lpszBuffer),pfile);
			fclose(pfile);
#ifdef WIN32
			ShellExecute(NULL,"open",sProgram.c_str(),"-P",m_sModulePath.c_str(),SW_HIDE);
#else
			sprintf(lpszBuffer,"\"%s\" -P &",sProgram.c_str());
			system(lpszBuffer);
#endif
		}
	}

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
	gApp->MyMain(nWaitSeconds,bService, sProtectDataFile);
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
			gApp->MyMain(nWaitSeconds,bService, sProtectDataFile);
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
			cService.AddToErrorMessageLog(TEXT("Failed start server£¡"));

			CGCApp::pointer gApp = CGCApp::create(m_sModulePath);
			gApp->MyMain(nWaitSeconds,bService, sProtectDataFile);
		}
	}
#else
	CGCApp::pointer gApp = CGCApp::create(m_sModulePath);
	gApp->MyMain(nWaitSeconds,bService, sProtectDataFile);
#endif
#endif // _DEBUG
#endif // USES_NEWVERSION
//	for (int i=0;i<30; i++)
//	{
//		if (boost::filesystem::remove(boost::filesystem::path(sProtectDataFile)))
//		{
//			break;
//		}
//#ifdef WIN32
//		Sleep(100);
//#else
//		usleep(100000);
//#endif
//	}
	return 0;
}

