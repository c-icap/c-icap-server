
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/
LOGDIR=$(localstatedir)/log/
SOCKDIR=/var/run/c-icap

RPATH_FLAG=
if USE_RPATH
RPATH_FLAG+=-rpath @libdir@
endif

#UTILS_LDADD = @THREADS_LDADD@ @DL_ADD_FLAG@ @ZLIB_LNDIR_LDADD@ @BZLIB_LNDIR_LDADD@ @BROTLI_LNDIR_LDADD@ @PCRE_LNDIR_LDADD@ @OPENSSL_LNDIR_LDADD@
UTILS_LDADD = @THREADS_LDADD@ @DL_ADD_FLAG@ $(EXT_PROGRAMS_MKLIB)

bin_PROGRAMS = c-icap-client c-icap-stretch
if USEBDB
bin_PROGRAMS += c-icap-mkbdb
endif

if USELMDB
bin_PROGRAMS += c-icap-mklmdb
endif

#other .....
c_icap_client_SOURCES = c-icap-client.c
c_icap_client_CFLAGS= -I$(top_srcdir)/include/ -I$(top_srcdir)/ -I$(top_builddir)/include/
c_icap_client_LDADD= $(top_builddir)/libicapapi.la $(UTILS_LDADD)
c_icap_client_LDFLAGS = -rdynamic $(RPATH_FLAG) @THREADS_LDFLAGS@

c_icap_mkbdb_SOURCES = c-icap-mkbdb.c
c_icap_mkbdb_CFLAGS=  -I$(top_srcdir)/include/ -I$(top_srcdir)/ -I$(top_builddir)/include/  @BDB_ADD_FLAG@
c_icap_mkbdb_LDADD= $(top_builddir)/libicapapi.la $(UTILS_LDADD) @BDB_ADD_LDADD@
c_icap_mkbdb_LDFLAGS = -rdynamic $(RPATH_FLAG) @THREADS_LDFLAGS@

if USELMDB
c_icap_mklmdb_SOURCES = c-icap-mklmdb.c
c_icap_mklmdb_CFLAGS=  -I$(top_srcdir)/include/ -I$(top_srcdir)/ -I$(top_builddir)/include/  @LMDB_ADD_FLAG@
c_icap_mklmdb_LDADD= $(top_builddir)/libicapapi.la $(UTILS_LDADD) @LMDB_ADD_LDADD@
c_icap_mklmdb_LDFLAGS = -rdynamic $(RPATH_FLAG) @THREADS_LDFLAGS@
endif

c_icap_stretch_SOURCES = c-icap-stretch.c
c_icap_stretch_CFLAGS= -I$(top_srcdir)/include/ -I$(top_srcdir)/ -I$(top_builddir)/include/
c_icap_stretch_LDADD = $(top_builddir)/libicapapi.la $(UTILS_LDADD)
c_icap_stretch_LDFLAGS= -rdynamic $(RPATH_FLAG) @THREADS_LDFLAGS@


