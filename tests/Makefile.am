
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/

RPATH_FLAG=
if USE_RPATH
RPATH_FLAG+=-rpath @libdir@
endif

AM_CFLAGS=-I$(top_srcdir)/ -I$(top_srcdir)/include/ -I$(top_builddir)/include/
AM_LDFLAGS = -rdynamic $(RPATH_FLAG) @THREADS_LDFLAGS@
LDADD = ../libicapapi.la @THREADS_LDADD@ @DL_ADD_FLAG@ $(EXT_PROGRAMS_MKLIB)

if USE_CPLUSPLUS
AM_CXXFLAGS = $(AM_CFLAGS)
CXX_PRGS = test_atomics_cplusplus
test_atomics_cplusplus_SOURCES = test_atomics_cplusplus.cc
endif

noinst_PROGRAMS = test_cache test_tables test_headers test_allocators test_arrays test_lists test_md5 test_base64 test_body test_ops test_filetype test_shared_locking test_atomics $(CXX_PRGS)
