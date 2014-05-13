
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@ 
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/
AM_CFLAGS= -I../include/
AM_LDFLAGS = -rdynamic -rpath @libdir@ @THREADS_LDFLAGS@
LDADD = ../libicapapi.la @THREADS_LDADD@ @DL_ADD_FLAG@ 

noinst_PROGRAMS = test_cache test_tables test_headers test_allocators test_arrays test_lists test_md5 test_base64 test_body
