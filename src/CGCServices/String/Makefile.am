lib_LTLIBRARIES = libStringService.la
THESOURCES = StringService.cpp Base64.cpp d3des.c
libStringService_la_SOURCES = $(THESOURCES)

libStringService_la_HEADERS = cgcString.h

AM_CFLAGS=-fPIC -shared -Wall -lssl -lcrypto
AM_CXXFLAGS=-fPIC -shared -Wall -lssl -lcrypto

libStringService_ladir = $(includedir)/CGCServices/String

