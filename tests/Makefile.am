
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@ 
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/
AM_CFLAGS= -I../include/
AM_LDFLAGS = -rdynamic -rpath @libdir@ @THREADS_LDFLAGS@
LIBS=@THREADS_LDADD@ ../libicapapi.la @DL_ADD_FLAG@

noinst_PROGRAMS = test_cache test_tables test_headers test_allocators test_arrays
