
pkglib_LTLIBRARIES=srv_ex206.la

AM_CPPFLAGS=-I$(top_srcdir)/ -I$(top_srcdir)/include/ -I$(top_builddir)/include/

if ISCYGWIN
MODS_LIB_ADD=$(top_builddir)/libicapapi.la
else
MODS_LIB_ADD=
endif

srv_ex206_la_LIBADD = $(MODS_LIB_ADD)
srv_ex206_la_CFLAGS=  @MODULES_CFLAGS@
srv_ex206_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@
srv_ex206_la_SOURCES = srv_ex206.c

