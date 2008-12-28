
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@ 
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/
AM_CFLAGS= -Iinclude/
AM_LDFLAGS = -rdynamic -rpath @libdir@ @THREADS_LDFLAGS@
LIBS=@THREADS_LDADD@ ../libicapapi.la @DL_ADD_FLAG@


bin_PROGRAMS = test_cache

