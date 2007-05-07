
pkglib_LTLIBRARIES=srv_echo.la
INCLUDES = -I$(top_srcdir)

srv_echo_la_LIBADD = @MODULES_LIBADD@
srv_echo_la_CFLAGS=  -I../../include/
srv_echo_la_LDFLAGS= -module -avoid-version
srv_echo_la_SOURCES = srv_echo.c


EXTRA_DIST= makefile.w32 srv_echo.def
