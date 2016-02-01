lib_LTLIBRARIES = libHttpServer.la
THESOURCES = CgcTcpClient.cpp HttpServer.cpp FileScripts.cpp ScriptItem.cpp MycpHttpServer.cpp
libHttpServer_la_SOURCES = $(THESOURCES)

AM_CFLAGS=-fPIC -shared -Wall -lssl -lcrypto
AM_CXXFLAGS=-fPIC -shared -Wall -lssl -lcrypto
