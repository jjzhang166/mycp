lib_LIBRARIES = libCGCLib.a

THESOURCES = CgcUdpClient.cpp CgcTcpClient.cpp \
	CgcRtpClient.cpp CgcBaseClient.cpp SotpClient.cpp
        
libCGCLib_a_SOURCES = $(THESOURCES)

libCGCLib_a_HEADERS = cgc_sotpclient.h cgcaddress.h CgcBaseClient.h CgcClientHandler.h CgcData.h \
	CGCLib.h CgcRtpClient.h CgcTcpClient.h CgcUdpClient.h dlldefine.h \
	IncludeBase.h SotpClient.h

AM_CFLAGS=-fPIC -Wall
AM_CXXFLAGS=-fPIC -Wall

libCGCLib_adir = $(includedir)/CGCLib

