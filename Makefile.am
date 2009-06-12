
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/
LOGDIR=$(localstatedir)/log/
SOCKDIR=/var/run/c-icap

DOXYGEN=@doxygen_bin@


SUBDIRS =  . utils modules services tests

lib_LTLIBRARIES=libicapapi.la
bin_PROGRAMS = c-icap

UTIL_SOURCES=net_io.c os/unix/net_io.c os/unix/proc_mutex.c os/unix/shared_mem.c os/unix/threads.c os/unix/utilfunc.c os/unix/proc_utils.c os/unix/dlib.c



libicapapi_la_SOURCES=  header.c body.c decode.c simple_api.c request_common.c \
                        filetype.c debug.c cfg_lib.c mem.c  service_lib.c \
                        cache.c lookup_table.c lookup_file_table.c hash.c \
			txt_format.c stats.c \
			$(UTIL_SOURCES)
c_icap_SOURCES = aserver.c request.c cfg_param.c \
                   proc_threads_queues.c http_auth.c \
                   access.c log.c service.c module.c \
		   commands.c mpmt_server.c dlib.c info.c


# libicapapi ......
libicapapi_la_CFLAGS= -Iinclude/ @ZLIB_ADD_FLAG@ -DCI_BUILD_LIB

libicapapi_la_LIBADD = @ZLIB_ADD_LDFLAG@
libicapapi_la_LDFLAGS= -shared -version-info 0:1:0


#c_icap the main server 
c_icap_DEPENDENCIES=libicapapi.la
c_icap_CFLAGS= -Iinclude/ -DCONFDIR=\"$(CONFIGDIR)\" -DMODSDIR=\"$(MODULESDIR)\" \
                          -DSERVDIR=\"$(SERVICESDIR)\" -DLOGDIR=\"$(LOGDIR)\"

c_icap_LDADD = @THREADS_LDADD@ libicapapi.la @DL_ADD_FLAG@
c_icap_LDFLAGS = -rdynamic -rpath @libdir@ @THREADS_LDFLAGS@



INCS = access.h body.h cfg_param.h c-icap-conf.h c-icap.h ci_threads.h \
	commands.h debug.h dlib.h filetype.h header.h log.h mem.h module.h \
	net_io.h proc_mutex.h proc_threads_queues.h request.h service.h \
	shared_mem.h simple_api.h util.h lookup_table.h hash.h stats.h

ALL_INCS=$(INCS:%.h=include/%.h)

pkginclude_HEADERS = $(ALL_INCS)
#sysconf_DATA = c-icap.conf c-icap.magic

doc:
	$(DOXYGEN) c-icap.dox

install-data-local:
	$(INSTALL) c-icap-config $(DESTDIR)$(bindir)/c-icap-config
	$(mkinstalldirs) $(DESTDIR)$(CONFIGDIR);
	$(INSTALL) c-icap.conf $(DESTDIR)$(CONFIGDIR)/c-icap.conf.default
	$(INSTALL) c-icap.magic $(DESTDIR)$(CONFIGDIR)/c-icap.magic.default
	if test ! -f $(DESTDIR)$(CONFIGDIR)/c-icap.conf; then  $(INSTALL) c-icap.conf $(DESTDIR)$(CONFIGDIR)/c-icap.conf; fi
	if test ! -f $(DESTDIR)$(CONFIGDIR)/c-icap.magic; then $(INSTALL) c-icap.magic $(DESTDIR)$(CONFIGDIR)/c-icap.magic; fi
	$(mkinstalldirs)  $(DESTDIR)$(LOGDIR); 
	$(mkinstalldirs)  $(DESTDIR)$(SOCKDIR);
	chgrp nobody $(DESTDIR)$(LOGDIR)
	chmod 775 $(DESTDIR)$(LOGDIR)
	chgrp nobody $(DESTDIR)$(SOCKDIR)
	chmod 775 $(DESTDIR)$(SOCKDIR)

EXTRA_DIST= RECONF INSTALL.txt config-w32.h makefile.w32 c-icap.conf c_icap_dll.mak c-icap.conf.in c-icap.magic  c_icap.mak c_icap.def \
             contrib/get_file.pl contrib/convert_old_magic.pl \
             winnt_server.c  os/win32/dll_entry.c os/win32/makefile.w32 os/win32/net_io.c os/win32/proc_mutex.c\
             os/win32/shared_mem.c os/win32/threads.c os/win32/utilfunc.c

