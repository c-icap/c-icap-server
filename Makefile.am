
CONFIGDIR=@configdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
CONFIGFILE=$(sysconfdir)/c-icap.conf
LOGDIR=$(localstatedir)/log/



SUBDIRS = os . modules services

lib_LTLIBRARIES=libicapapi.la
bin_PROGRAMS = c-icap icap-client icap-stretch

libicapapi_la_SOURCES= header.c body.c base64.c simple_api.c

c_icap_SOURCES = aserver.c request.c service.c module.c debug.c log.c proc_threads_queues.c cfg_param.c net_io.c access.c  mpmt_server.c

libicapapi_la_CFLAGS= -Iinclude/
libicapapi_la_LDFLAGS= -shared -version-info 0:1:0

c_icap_DEPENDENCIES=os/unix/libutils.a libicapapi.la
c_icap_CFLAGS= -Iinclude/ -DCONFFILE=\"$(CONFIGFILE)\" -DMODSDIR=\"$(MODULESDIR)\" -DSERVDIR=\"$(SERVICESDIR)\" -DLOGDIR=\"$(LOGDIR)\"
#c_icap_LDADD = -L. -licapapi -Los/unix -lutils -lpthread @DL_ADD_FLAG@
c_icap_LDADD =  libicapapi.la -Los/unix -lutils -lpthread @DL_ADD_FLAG@
c_icap_LDFLAGS = -rdynamic -rpath @libdir@
#c_icap_LDFLAGS = -rpath `pwd`/.libs/



icap_client_SOURCES = icap-client.c

icap_stretch_SOURCES = icap-stretch.c
icap_stretch_CFLAGS= -Iinclude/
icap_stretch_LDADD = -lpthread @DL_ADD_FLAG@

INCS = access.h body.h cfg_param.h c-icap.h ci_threads.h debug.h header.h log.h module.h\
       net_io.h proc_mutex.h proc_threads_queues.h request.h service.h shared_mem.h simple_api.h

ALL_INCS=$(INCS:%.h=include/%.h)

pkginclude_HEADERS = $(ALL_INCS)
sysconf_DATA = c-icap.conf

EXTRA_DIST= makefile.w32 c-icap.conf c_icap_dll.mak c-icap.conf.in

