lib_LTLIBRARIES = libHttpServer.la
THESOURCES = CgcTcpClient.cpp HttpServer.cpp FileScripts.cpp ScriptItem.cpp MycpHttpServer.cpp
libHttpServer_la_SOURCES = $(THESOURCES)
#libHttpServer_la_LIBADD = $(libdir)/libCGCLib.a	$(libdir)/libCGCClass.a

AM_CFLAGS=-fPIE -shared -Wall -lssl -lcrypto
AM_CXXFLAGS=-fPIE -shared -Wall -lssl -lcrypto
