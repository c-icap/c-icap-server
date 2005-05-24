
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/
LOGDIR=$(localstatedir)/log/


SUBDIRS =  . modules services

lib_LTLIBRARIES=libicapapi.la
bin_PROGRAMS = c-icap icap-client icap-stretch

UTIL_SOURCES=os/unix/net_io.c os/unix/proc_mutex.c os/unix/shared_mem.c os/unix/utilfunc.c os/unix/proc_utils.c



libicapapi_la_SOURCES=  header.c body.c base64.c simple_api.c \
                          filetype.c debug.c cfg_lib.c  $(UTIL_SOURCES)
c_icap_SOURCES = aserver.c request.c cfg_param.c net_io.c \
                   proc_threads_queues.c http_auth.c \
                   access.c log.c service.c module.c  mpmt_server.c  


# libicapapi ......
libicapapi_la_CFLAGS= -Iinclude/ -DCI_BUILD_LIB


libicapapi_la_LDFLAGS= -shared -version-info 0:1:0


#c_icap the main server 
c_icap_DEPENDENCIES=libicapapi.la
c_icap_CFLAGS= -Iinclude/ -DCONFDIR=\"$(CONFIGDIR)\" -DMODSDIR=\"$(MODULESDIR)\" \
                          -DSERVDIR=\"$(SERVICESDIR)\" -DLOGDIR=\"$(LOGDIR)\"

c_icap_LDADD = -lpthread libicapapi.la @DL_ADD_FLAG@
c_icap_LDFLAGS = -rdynamic -rpath @libdir@



#other .....
icap_client_SOURCES = icap-client.c
icap_client_CFLAGS= -Iinclude/
icap_client_LDADD= libicapapi.la @DL_ADD_FLAG@
icap_client_LDFLAGS = -rdynamic -rpath @libdir@

icap_stretch_SOURCES = icap-stretch.c
icap_stretch_CFLAGS= -Iinclude/
icap_stretch_LDADD = -lpthread @DL_ADD_FLAG@

INCS = access.h body.h cfg_param.h c-icap.h ci_threads.h debug.h header.h log.h module.h\
       net_io.h proc_mutex.h proc_threads_queues.h request.h service.h shared_mem.h simple_api.h util.h filetype.h

ALL_INCS=$(INCS:%.h=include/%.h)

pkginclude_HEADERS = $(ALL_INCS)
sysconf_DATA = c-icap.conf c-icap.magic

install-data-local:
	$(mkinstalldirs) $(LOGDIR); chgrp nobody $(LOGDIR);chmod 775 $(LOGDIR)

EXTRA_DIST= RECONF INSTALL.txt config-w32.h makefile.w32 c-icap.conf c_icap_dll.mak c-icap.conf.in c-icap.magic  c_icap.mak c_icap.def contrib/get_file.pl \
             winnt_server.c  os/win32/dll_entry.c os/win32/makefile.w32 os/win32/net_io.c os/win32/proc_mutex.c\
             os/win32/shared_mem.c os/win32/threads.c os/win32/utilfunc.c

