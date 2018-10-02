
pkglib_LTLIBRARIES=srv_ex206.la

AM_CPPFLAGS=-I$(top_srcdir)/ -I$(top_srcdir)/include/ -I$(top_builddir)/include/

srv_ex206_la_LIBADD = @MODULES_LIBADD@
srv_ex206_la_CFLAGS=  @MODULES_CFLAGS@ @OPENSSL_ADD_FLAG@
srv_ex206_la_LDFLAGS= -module -avoid-version
srv_ex206_la_SOURCES = srv_ex206.c

