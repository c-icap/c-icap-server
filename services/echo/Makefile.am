
pkglib_LTLIBRARIES=srv_echo.la

AM_CPPFLAGS=-I$(top_srcdir)/ -I$(top_srcdir)/include/ -I$(top_builddir)/include/

if ISCYGWIN
MODS_LIB_ADD=$(top_builddir)/libicapapi.la
else
MODS_LIB_ADD=
endif

srv_echo_la_LIBADD = $(MODS_LIB_ADD)
srv_echo_la_CFLAGS=  @MODULES_CFLAGS@
srv_echo_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@
srv_echo_la_SOURCES = srv_echo.c


EXTRA_DIST= makefile.w32 srv_echo.def
