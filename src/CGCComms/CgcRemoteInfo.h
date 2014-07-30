/*
    CGCP is a C++ General Communication Platform software library.
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

// CgcRemoteInfo.h file here
#ifndef __CgcRemoteInfo_h__
#define __CgcRemoteInfo_h__

#include <CGCBase/cgcCommunications.h>

class CCommEventData
{
public:
	enum CommEventType
	{
		CET_Accept	= 1
		, CET_Recv	= 2
		, CET_Close	= 3
		, CET_Exception	= 4
		//, CET_Other	= 0xff
	};

private:
	CommEventType m_cet;
	//void * m_remote;
	cgcRemote::pointer m_remote;
	unsigned long m_remoteId;
	unsigned char * m_recvData;
	unsigned int m_recvSize;
	int m_nErrorCode;
	std::string m_sCodeMessage;
public:
//	void setCommEventType(CommEventType newv) {m_cet = newv;}
	CommEventType getCommEventType(void) const {return m_cet;}

	//void setRemote(void * newv) {m_remote = newv;}
	//const void * getRemote(void) const {return m_remote;}
	void setRemote(cgcRemote::pointer newv) {m_remote = newv;}
	cgcRemote::pointer getRemote(void) const {return m_remote;}

	void setRemoteId(unsigned long newv) {m_remoteId = newv;}
	unsigned long getRemoteId(void) const {return m_remoteId;}

	void setRecvData(const unsigned char * recvData, unsigned int recvSize)
	{
		if (recvData == 0 || recvSize == 0)
			return;
		clearData();

		m_recvSize = recvSize;
		m_recvData = new unsigned char[m_recvSize+1];
		memcpy(m_recvData, recvData, m_recvSize);
		m_recvData[m_recvSize] = '\0';
	}
	const unsigned char * getRecvData(void) const {return m_recvData;}
	unsigned int getRecvSize(void) const {return m_recvSize;}
	int GetErrorCode(void) const {return m_nErrorCode;}
	void SetCodeMessage(const std::string& sMessage) {m_sCodeMessage=sMessage;}
	const std::string& GetCodeMessage(void) const {return m_sCodeMessage;}

	void clearData(void)
	{
		m_recvSize = 0;
		if (m_recvData)
		{
			delete[] m_recvData;
			m_recvData = 0;
		}
	}

public:
	CCommEventData(CommEventType commEventType,int nErrorCode=0)
		: m_cet(commEventType)
		//, m_remote(0)
		, m_remoteId(0)
		, m_recvData(0)
		, m_recvSize(0)
		, m_nErrorCode(nErrorCode)
	{
	}
	virtual ~CCommEventData(void)
	{
		clearData();
	}
};

#endif // __CgcRemoteInfo_h__
