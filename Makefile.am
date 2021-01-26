srcdir = @srcdir@
top_builddir=@top_builddir@

CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
#CONFIGDIR=$(sysconfdir)/
DATADIR=$(pkgdatadir)/
LOGDIR=$(localstatedir)/log/
SOCKDIR=/var/run/c-icap

DOXYGEN=@doxygen_bin@

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =  . utils modules services tests docs


lib_LTLIBRARIES=libicapapi.la libcicap.la
bin_PROGRAMS = c-icap

bin_SCRIPTS = c-icap-config c-icap-libicapapi-config

UTIL_LIB_SOURCES=net_io.c util.c os/unix/net_io.c os/unix/proc_mutex.c os/unix/shared_mem.c os/unix/threads.c os/unix/utilfunc.c os/unix/dlib.c
if USE_OPENSSL
UTIL_LIB_SOURCES += openssl/net_io_ssl.c
endif
if USE_REGEX
UTIL_LIB_SOURCES += regex.c
endif
UTIL_SOURCES=os/unix/proc_utils.c

CICAP_CFG_SOURCES=_autoconf_table.c _c_icap_conf_table.c

RPATH_FLAG=
if USE_RPATH
RPATH_FLAG+=-rpath @libdir@
endif

if ISCYGWIN
libcicap_la_SOURCES = cicapcalls.c
endif

libicapapi_la_SOURCES=  header.c body.c decode.c encode.c simple_api.c request_common.c \
                        filetype.c debug.c cfg_lib.c mem.c  service_lib.c \
                        cache.c lookup_table.c lookup_file_table.c hash.c \
			txt_format.c stats.c types_ops.c acl.c txtTemplate.c \
			array.c registry.c md5.c client.c $(UTIL_LIB_SOURCES)

c_icap_SOURCES = aserver.c request.c cfg_param.c \
                   proc_threads_queues.c http_auth.c \
                   access.c log.c service.c module.c \
		   commands.c mpmt_server.c dlib.c info.c \
		   default_acl.c port.c $(UTIL_SOURCES) $(CICAP_CFG_SOURCES)


# libicapapi ......
libicapapi_la_CFLAGS= $(INVISIBILITY_CFLAG) -I$(srcdir)/include/ -Iinclude/ @ZLIB_ADD_FLAG@ @OPENSSL_ADD_FLAG@ @BZLIB_ADD_FLAG@ @BROTLI_ADD_FLAG@ @PCRE_ADD_FLAG@ -DCI_BUILD_LIB

libicapapi_la_LIBADD = @ZLIB_ADD_LDADD@ @BZLIB_ADD_LDADD@ @BROTLI_ADD_LDADD@ @PCRE_ADD_LDADD@ @DL_ADD_FLAG@ @THREADS_LDADD@ @OPENSSL_ADD_LDADD@
libicapapi_la_LDFLAGS= -shared -version-info @CICAPLIB_VERSION@ @LIBS_LDFLAGS@ @THREADS_LDFLAGS@

if ISCYGWIN
libcicap_la_CFLAGS =  $(INVISIBILITY_CFLAG) -I$(srcdir)/include/ -Iinclude/  -DCI_BUILD_LIB

libcicap_la_LIBADD =
libcicap_la_LDFLAGS =  -shared -version-info @CICAPLIB_VERSION@ @LIBS_LDFLAGS@
endif

export EXT_PROGRAMS_MKLIB = @ZLIB_LNDIR_LDADD@ @BZLIB_LNDIR_LDADD@ @BROTLI_LNDIR_LDADD@ @PCRE_LNDIR_LDADD@ @OPENSSL_LNDIR_LDADD@

#c_icap the main server
c_icap_DEPENDENCIES=libicapapi.la
c_icap_CFLAGS= $(INVISIBILITY_CFLAG) -I$(top_srcdir)/include/ -I$(top_builddir)/include/ \
                        -DCI_CONFDIR=\"$(CONFIGDIR)\" -DCI_MODSDIR=\"$(MODULESDIR)\" \
                        -DCI_SERVDIR=\"$(SERVICESDIR)\" -DCI_LOGDIR=\"$(LOGDIR)\"  \
                        -DCI_DATADIR=\"$(DATADIR)\" @OPENSSL_ADD_FLAG@

c_icap_LDADD = libicapapi.la @DL_ADD_FLAG@ @THREADS_LDADD@ $(EXT_PROGRAMS_MKLIB)
c_icap_LDFLAGS = -rdynamic $(RPATH_FLAG) @THREADS_LDFLAGS@


INCS = access.h body.h cfg_param.h c-icap-conf.h c-icap.h ci_threads.h \
	commands.h debug.h dlib.h filetype.h header.h log.h mem.h module.h \
	net_io.h proc_mutex.h proc_threads_queues.h request.h service.h \
	shared_mem.h simple_api.h util.h lookup_table.h hash.h stats.h acl.h \
        cache.h txt_format.h types_ops.h txtTemplate.h array.h registry.h \
	md5.h ci_regex.h net_io_ssl.h openssl_support.h port.h encoding.h \
	request_util.h client.h

ALL_INCS=$(INCS:%.h=include/%.h)

pkginclude_HEADERS = $(ALL_INCS)


do_subst=sed -e 's%[@]SYSCONFDIR[@]%$(CONFIGDIR)%g' \
             -e 's%[@]PACKAGE_VERSION[@]%$(PACKAGE_VERSION)%g' \
             -e 's%[@]PACKAGE[@]%$(PACKAGE)%g' \
             -e 's%[@]prefix[@]%$(prefix)%g' \
             -e 's%[@]LIBDIR[@]%$(libdir)%g' \
             -e 's%[@]PKGINCLUDEDIR[@]%$(pkgincludedir)%g' \
             -e 's%[@]INCLUDEDIR[@]%$(includedir)%g' \
             -e 's%[@]PKGLIBDIR[@]%$(pkglibdir)%g' \
             -e 's%[@]PKGDATADIR[@]%$(pkgdatadir)%g' \
             -e 's%[@]CFLAGS[@]%$(CFLAGS)%g' \
             -e 's%[@]MODULES_LIBADD[@]%$(MODULES_LIBADD)%g' \
             -e 's%[@]MODULES_CFLAGS[@]%$(MODULES_CFLAGS)%g' \
             -e 's%[@]EXT_PROGRAMS_LIBADD[@]%$(EXT_PROGRAMS_MKLIB)%g' \
             -e 's%[@]SOCKDIR[@]%$(SOCKDIR)%g'

CLEANFILES = c-icap-config c-icap-libicapapi-config

_autoconf_table.c: build/c_icap_autoconf.awk autoconf.h
	awk -f build/c_icap_autoconf.awk -vinc=autoconf.h -vname=autoconf < autoconf.h > $@

_c_icap_conf_table.c: build/c_icap_autoconf.awk include/c-icap-conf.h
	awk -f build/c_icap_autoconf.awk -vinc=c-icap-conf.h -vname=c_icap_conf < include/c-icap-conf.h > $@

# The c-icap.conf, c-icap-config, and c-icap-libicapapi-config must rebuild
# on every new configure run. The include/c-icap-conf.h is rebuild when
# configure runs so it is a good test.
c-icap.conf: c-icap.conf.in include/c-icap-conf.h
	$(do_subst) < $(srcdir)/c-icap.conf.in > $@

c-icap-config: c-icap-config.in include/c-icap-conf.h
	$(do_subst) < $(srcdir)/c-icap-config.in > $@
	chmod 755 $@

c-icap-libicapapi-config:  c-icap-libicapapi-config.in include/c-icap-conf.h
	$(do_subst) < $(srcdir)/c-icap-libicapapi-config.in > $@
	chmod 755 $@

doc:
	$(DOXYGEN) $(srcdir)/c-icap.dox

install-data-local: c-icap.conf
	$(mkinstalldirs) $(DESTDIR)$(CONFIGDIR);
	$(INSTALL) c-icap.conf $(DESTDIR)$(CONFIGDIR)/c-icap.conf.default
	$(INSTALL) $(srcdir)/c-icap.magic $(DESTDIR)$(CONFIGDIR)/c-icap.magic.default
	if test ! -f $(DESTDIR)$(CONFIGDIR)/c-icap.conf; then  $(INSTALL) c-icap.conf $(DESTDIR)$(CONFIGDIR)/c-icap.conf; fi
	if test ! -f $(DESTDIR)$(CONFIGDIR)/c-icap.magic; then $(INSTALL) $(srcdir)/c-icap.magic $(DESTDIR)$(CONFIGDIR)/c-icap.magic; fi
	$(mkinstalldirs)  $(DESTDIR)$(LOGDIR);
	$(mkinstalldirs)  $(DESTDIR)$(SOCKDIR);
	chgrp nogroup $(DESTDIR)$(LOGDIR) || echo -e "*********\nWARNING! Can not set group for the log dir $(DESTDIR)$(LOGDIR)\n*********\n"
	chmod 775 $(DESTDIR)$(LOGDIR)
	chgrp nogroup $(DESTDIR)$(SOCKDIR) || echo -e "*********\nWARNING! Can not set group for the c-icap socket store dir $(DESTDIR)$(SOCKDIR)\n\n*********\n"
	chmod 775 $(DESTDIR)$(SOCKDIR)

EXTRA_DIST = RECONF config-w32.h makefile.w32 \
             c_icap_dll.mak c-icap.conf.in c-icap.magic  c_icap.mak c_icap.def \
             contrib/get_file.pl contrib/convert_old_magic.pl \
             winnt_server.c  os/win32/dll_entry.c os/win32/makefile.w32 \
             os/win32/net_io.c os/win32/proc_mutex.c \
             os/win32/shared_mem.c os/win32/threads.c os/win32/utilfunc.c \
             common.h \
             c-icap-config.in c-icap-libicapapi-config.in c-icap.dox \
             build/c_icap_version.awk \
	     openssl/build_openssl_opts.pl \
	     openssl/openssl_options.c

