

# lib_LTLIBRARIES=sys_logger.la perl_handler.la

pkglib_LTLIBRARIES= sys_logger.la dnsbl_tables.la shared_cache.la
if USEPERL
pkglib_LTLIBRARIES += perl_handler.la
endif

if USEBDB
pkglib_LTLIBRARIES += bdb_tables.la
endif

if USELDAP
pkglib_LTLIBRARIES += ldap_module.la
endif

if USEMEMCACHED
pkglib_LTLIBRARIES += memcached_cache.la
endif

if USELMDB
pkglib_LTLIBRARIES += lmdb_tables.la
endif

AM_CPPFLAGS=-I$(top_srcdir)/ -I$(top_srcdir)/include/ -I$(top_builddir)/include/

if ISCYGWIN
MODS_LIB_ADD=$(top_builddir)/libicapapi.la $(top_builddir)/libcicap.la
else
MODS_LIB_ADD=
endif

sys_logger_la_LIBADD = $(MODS_LIB_ADD)
sys_logger_la_CFLAGS=  @MODULES_CFLAGS@
sys_logger_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@
sys_logger_la_SOURCES = sys_logger.c

dnsbl_tables_la_LIBADD = $(MODS_LIB_ADD)
dnsbl_tables_la_CFLAGS=  @MODULES_CFLAGS@
dnsbl_tables_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@
dnsbl_tables_la_SOURCES = dnsbl_tables.c

perl_handler_la_LIBADD = $(MODS_LIB_ADD) @perllib@  -L@perlcore@ -lperl
perl_handler_la_CFLAGS=  @MODULES_CFLAGS@ @perlccflags@ -I@perlcore@
perl_handler_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@ @perlldflags@
perl_handler_la_SOURCES = perl_handler.c


bdb_tables_la_LIBADD = $(MODS_LIB_ADD) @BDB_ADD_LDADD@
bdb_tables_la_CFLAGS=  @MODULES_CFLAGS@ @BDB_ADD_FLAG@
bdb_tables_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@
bdb_tables_la_SOURCES = bdb_tables.c

lmdb_tables_la_LIBADD = $(MODS_LIB_ADD) @LMDB_ADD_LDADD@
lmdb_tables_la_CFLAGS=  @MODULES_CFLAGS@ @LMDB_ADD_FLAG@
lmdb_tables_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@
lmdb_tables_la_SOURCES = lmdb_tables.c

ldap_module_la_LIBADD = $(MODS_LIB_ADD) @LDAP_ADD_LDADD@
ldap_module_la_CFLAGS=  @MODULES_CFLAGS@ @LDAP_ADD_FLAG@
ldap_module_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@
ldap_module_la_SOURCES = ldap_module.c

memcached_cache_la_LIBADD=  $(MODS_LIB_ADD) @MEMCACHED_ADD_LDADD@
memcached_cache_la_CFLAGS=  @MODULES_CFLAGS@ @MEMCACHED_ADD_FLAG@
memcached_cache_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@
memcached_cache_la_SOURCES= memcached.c

shared_cache_la_LIBADD= $(MODS_LIB_ADD)
shared_cache_la_CFLAGS=
shared_cache_la_LDFLAGS= -module -avoid-version @LIBS_LDFLAGS@
shared_cache_la_SOURCES= shared_cache.c
