lib_LTLIBRARIES = libHttpService.la
THESOURCES = HttpService.cpp CgcTcpClient.cpp
libHttpService_la_SOURCES = $(THESOURCES)

libHttpService_la_LIBADD = $(libdir)/libboost_thread.a $(libdir)/libboost_filesystem.a $(libdir)/libboost_system.a
AM_CFLAGS=-fPIC -shared -Wall -lssl -lcrypto
AM_CXXFLAGS=-fPIC -shared -Wall -lssl -lcrypto

