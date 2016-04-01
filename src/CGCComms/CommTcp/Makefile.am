lib_LTLIBRARIES = libCommTcpServer.la
THESOURCES = CommTcpServer.cpp
libCommTcpServer_la_SOURCES = $(THESOURCES)

libCommTcpServer_la_LIBADD = ../../CGCClass/libCGCClass.a

AM_CFLAGS=-fPIE -shared -Wall -lssl -lcrypto
AM_CXXFLAGS=-fPIE -shared -Wall -lssl -lcrypto

