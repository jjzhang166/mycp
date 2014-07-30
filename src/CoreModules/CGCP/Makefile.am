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

AM_CFLAGS = -lboost_system -lboost_thread -lboost_filesystem -ldl
AM_CXXFLAGS = -lboost_system -lboost_thread -lboost_filesystem -ldl

CGCP_LDADD = $(libdir)/libCGCLib.a $(libdir)/libCGCClass.a

