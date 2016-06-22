lib_LTLIBRARIES = libSqliteService.la
THESOURCES = SqliteService.cpp
libSqliteService_la_SOURCES = $(THESOURCES)

libSqliteService_la_LIBADD = $(libdir)/libboost_thread.a $(libdir)/libboost_filesystem.a $(libdir)/libboost_system.a \
	/usr/local/lib/libsqlite3.a

# **32 OK,need libmysqlclient.so.16
#AM_CFLAGS=-fPIC -shared -Wall -L/usr/lib/mysql -lmysqlclient
#AM_CXXFLAGS=-fPIC -shared -Wall -L/usr/lib/mysql -lmysqlclient

# **64 OK,need libmysql.so.16
AM_CFLAGS=-fPIC -shared -Wall
AM_CXXFLAGS=-fPIC -shared -Wall

