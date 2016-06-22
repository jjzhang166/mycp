lib_LTLIBRARIES = libBodbService.la
THESOURCES = BodbService.cpp
libBodbService_la_SOURCES = $(THESOURCES)

libBodbService_la_LIBADD = $(libdir)/libboost_thread.a $(libdir)/libboost_filesystem.a $(libdir)/libboost_system.a \
	$(libdir)/libbodb.a $(libdir)/libsqlparser.a 

AM_CFLAGS=-fPIC -shared -Wall
AM_CXXFLAGS=-fPIC -shared -Wall

