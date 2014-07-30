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

// extern.h file here
#ifndef __extern_head__
#define __extern_head__

#include "cgcApplication.h"
#include "cgcSystem.h"
#include "cgcServices.h"

namespace cgc {

	extern cgc::cgcApplication::pointer theApplication;
	extern cgc::cgcSystem::pointer theSystem;
	extern cgc::cgcServiceManager::pointer theServiceManager;


} // cgc namespace


#endif // __extern_head__
