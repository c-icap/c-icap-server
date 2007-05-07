

# lib_LTLIBRARIES=sys_logger.la perl_handler.la

pkglib_LTLIBRARIES= sys_logger.la
if USEPERL
pkglib_LTLIBRARIES += perl_handler.la
endif


INCLUDES = -I$(top_srcdir)

sys_logger_la_LIBADD = @MODULES_LIBADD@
sys_logger_la_CFLAGS=  -I../include/
sys_logger_la_LDFLAGS= -module -avoid-version
sys_logger_la_SOURCES = sys_logger.c


perl_handler_la_LIBADD = @MODULES_LIBADD@ @perllib@  -L@perlcore@ -lperl
perl_handler_la_CFLAGS=  -I../include/ @perlccflags@ -I@perlcore@
perl_handler_la_LDFLAGS= -module -avoid-version @perlldflags@
perl_handler_la_SOURCES = perl_handler.c



