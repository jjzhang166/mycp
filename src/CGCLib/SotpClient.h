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

// SotpClient.h file here
#ifndef __SotpClient_h__
#define __SotpClient_h__

#include "dlldefine.h"
#include "CgcClientHandler.h"
#include "cgcaddress.h"

class LIBSOTPCLIENT_CLASS CSotpClient
{
public:
	DoSotpClientHandler::pointer startClient(const CCgcAddress & address, unsigned int bindPort=0);
	void stopClient(DoSotpClientHandler::pointer pDoHandler);
	void stopAllClient(void);

public:
	CSotpClient(void);
	virtual ~CSotpClient(void);

};

#endif // __SotpClient_h__
