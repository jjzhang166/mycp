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

#ifndef __cgcutils_head__
#define __cgcutils_head__

//#ifdef WIN32
//#pragma warning(disable:4996)
//#endif // WIN32


namespace cgc {

inline const tstring & replace_string(tstring & strSource, const tstring & strFind, const tstring &strReplace)
{
	std::string::size_type pos=0;
	std::string::size_type findlen=strFind.size();
	std::string::size_type replacelen=strReplace.size();
	while ((pos=strSource.find(strFind, pos)) != std::string::npos)
	{
		strSource.replace(pos, findlen, strReplace);
		pos += replacelen;
	}
	return strSource;
}
inline const tstring & replace_char(tstring & strSource, char cFind, const tstring &strReplace)
{
	std::string::size_type pos=0;
	std::string::size_type findlen=1;
	std::string::size_type replacelen=strReplace.size();
	while ((pos=strSource.find(cFind, pos)) != std::string::npos)
	{
		strSource.replace(pos, findlen, strReplace);
		pos += replacelen;
	}
	return strSource;
}

} // cgc namespace

#endif // __cgcutils_head__
