bin_PROGRAMS=DLLTestClient
THESOURCES = DLLTestClient.cpp

DLLTestClient_SOURCES = $(THESOURCES)

AM_CFLAGS=-Wall
AM_CXXFLAGS=-Wall

DLLTestClient_LDADD = $(libdir)/libCGCLib.a \
	$(libdir)/libCGCClass.a \
	-lboost_thread -lboost_filesystem

