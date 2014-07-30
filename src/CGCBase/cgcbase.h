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

#ifndef __cgcbase_head__
#define __cgcbase_head__

#ifdef WIN32
#pragma warning(disable:4996)
#endif // WIN32

#include "cgcApplication.h"
#include "cgcSystem.h"
#include "cgcServices.h"

namespace cgc {

	cgc::cgcApplication::pointer theApplication;
	extern "C" void CGC_API CGC_SetApplicationHandler(const cgc::cgcApplication::pointer& pHandler)
	{
		BOOST_ASSERT(pHandler.get() != NULL);
		theApplication = pHandler;
	}
#define CGC_LOG(X) theApplication->log X
#define CGC_APP_PARAMETER(N) theApplication->getInitParameter(N)
#define CGC_APP_VALUE(N, D) theApplication->getInitParameterValue(N, D)
#define CGC_APP_VALUE2(N, D) theApplication->getInitParameterValue2(N, D)

	cgc::cgcSystem::pointer theSystem;
	extern "C" void CGC_API CGC_SetSystemHandler(const cgc::cgcSystem::pointer& pHandler)
	{
		BOOST_ASSERT(pHandler.get() != NULL);
		theSystem = pHandler;
	}
#define CGC_SYS_PARAMETER(N) theSystem->getInitParameter(N)
#define CGC_SYS_VALUE(N, D) theSystem->getInitParameterValue(N, D)
//#define CGC_SYS_VALUE2(N, D) gSystem->getInitParameterValue2(N, D)

	cgc::cgcServiceManager::pointer theServiceManager;
	extern "C" void CGC_API CGC_SetServiceManagerHandler(const cgc::cgcServiceManager::pointer& pHandler)
	{
		BOOST_ASSERT(pHandler.get() != NULL);
		theServiceManager = pHandler;
	}

} // cgc namespace

#endif // __cgcbase_head__
