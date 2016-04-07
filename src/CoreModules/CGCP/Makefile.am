bin_PROGRAMS = CGCP

THESOURCES = \
	CGCP.cpp \
	CGC/TimerInfo.cpp \
	CGC/SessionMgr.cpp \
	CGC/SotpResponseImpl.cpp \
	CGC/HttpResponseImpl.cpp \
	CGC/ModuleMgr.cpp \
	CGC/CGCApp.cpp

CGCP_SOURCES = $(THESOURCES)

AM_CFLAGS = -g -lboost_system -lboost_thread -lboost_filesystem -ldl -lssl -lcrypto
AM_CXXFLAGS = -g -lboost_system -lboost_thread -lboost_filesystem -ldl -lssl -lcrypto

CGCP_LDADD = $(libdir)/libCGCLib.a $(libdir)/libCGCClass.a
CGCP_adir = /usr/include
