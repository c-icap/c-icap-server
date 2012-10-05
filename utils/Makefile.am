
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/
LOGDIR=$(localstatedir)/log/
SOCKDIR=/var/run/c-icap


bin_PROGRAMS = c-icap-client c-icap-stretch
if USEBDB
bin_PROGRAMS += c-icap-mkbdb
endif

#other .....
c_icap_client_SOURCES = c-icap-client.c
c_icap_client_CFLAGS= -I../include/
c_icap_client_LDADD= ../libicapapi.la @THREADS_LDADD@ @DL_ADD_FLAG@
c_icap_client_LDFLAGS = -rdynamic -rpath @libdir@ @THREADS_LDFLAGS@

c_icap_mkbdb_SOURCES = c-icap-mkbdb.c
c_icap_mkbdb_CFLAGS= -I../include/  @BDB_ADD_FLAG@
c_icap_mkbdb_LDADD= ../libicapapi.la @THREADS_LDADD@ @DL_ADD_FLAG@  @BDB_ADD_LDADD@
c_icap_mkbdb_LDFLAGS = -rdynamic -rpath @libdir@ @THREADS_LDFLAGS@ 

c_icap_stretch_SOURCES = c-icap-stretch.c
c_icap_stretch_CFLAGS= -I../include/
c_icap_stretch_LDADD = ../libicapapi.la @THREADS_LDADD@ @DL_ADD_FLAG@
c_icap_stretch_LDFLAGS= @THREADS_LDFLAGS@

