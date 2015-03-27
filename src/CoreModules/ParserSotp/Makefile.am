lib_LTLIBRARIES = libParserSotp.la
THESOURCES = ParserSotp.cpp
libParserSotp_la_SOURCES = $(THESOURCES)

libParserSotp_la_LIBADD = ../../CGCClass/libCGCClass.a

AM_CFLAGS=-fPIC -shared -Wall -lssl -lcrypto
AM_CXXFLAGS=-fPIC -shared -Wall -lssl -lcrypto

