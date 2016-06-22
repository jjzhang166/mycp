lib_LTLIBRARIES = libPGService.la
THESOURCES = PGService.cpp com/hash.c com/primes.c db/pool.c db/postsink.c db/sink.c
libPGService_la_SOURCES = $(THESOURCES)

#libPGService_la_LIBADD = $(libdir)/libboost_thread.a $(libdir)/libboost_filesystem.a $(libdir)/libboost_system.a $(libdir)/libpq.a
#AM_CFLAGS=-fPIC -shared -Wall
#AM_CXXFLAGS=-fPIC -shared -Wall
libPGService_la_LIBADD = $(libdir)/libboost_thread.a $(libdir)/libboost_filesystem.a $(libdir)/libboost_system.a
AM_CFLAGS=-fPIC -shared -Wall -L/usr/lib -lpq
AM_CXXFLAGS=-fPIC -shared -Wall -L/usr/lib -lpq
