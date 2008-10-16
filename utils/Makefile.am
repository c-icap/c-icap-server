
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/
LOGDIR=$(localstatedir)/log/
SOCKDIR=/var/run/c-icap


bin_PROGRAMS = icap-client icap-stretch

#other .....
icap_client_SOURCES = icap-client.c
icap_client_CFLAGS= -Iinclude/
icap_client_LDADD= @THREADS_LDADD@ ../libicapapi.la @DL_ADD_FLAG@
icap_client_LDFLAGS = -rdynamic -rpath @libdir@ @THREADS_LDFLAGS@

icap_stretch_SOURCES = icap-stretch.c
icap_stretch_CFLAGS= -Iinclude/
icap_stretch_LDADD = @THREADS_LDADD@ @DL_ADD_FLAG@
icap_stretch_LDFLAGS= @THREADS_LDFLAGS@

