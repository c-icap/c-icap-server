dnl Process this file with autoconf to produce a configure script.


dnl AC_INIT([c_icap], m4_normalize(m4_include([VERSION.m4])))
AC_INIT([c_icap], [0.6.2])
CICAPLIB_VERSION=6:2:0
AC_SUBST(CICAPLIB_VERSION)
dnl CICAPLIB_VERSION is the libtool current[:revision[:age]] version info
dnl libtool directions about version info
dnl - library source code has changed since the last update c:r:a => c:r+1:a
dnl - interfaces have been added, removed, or changed c:r:a => c+1:0:a
dnl - interfaces have been added c:r:a => c:r:a+1
dnl - interfaces have been removed  c:r:a => c:r:0

AC_CONFIG_SRCDIR(aserver.c)
AM_MAINTAINER_MODE
AC_CONFIG_HEADERS(autoconf.h)

AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_HOST

AC_USE_SYSTEM_EXTENSIONS

AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AM_PROG_AR

WORDS_BIGENDIAN="0"
WORDS_LITTLEENDIAN="0"
AC_C_BIGENDIAN(
[AC_DEFINE(WORDS_BIGENDIAN, 1,
          [Define WORDS_BIGENDIAN to 1 if your processor stores words with the most significant byte first (like Motorola and SPARC, unlike Intel).])]
[WORDS_BIGENDIAN="1"]
[WORDS_LITTLEENDIAN="0"]
          ,
[AC_DEFINE(WORDS_LITTLEENDIAN, 1,
          [Define WORDS_LITTLEENDIAN to 1 if your processor stores words with the least significant byte first (like Intel).])]
[WORDS_BIGENDIAN="0"]
[WORDS_LITTLEENDIAN="1"]
)
AC_SUBST(WORDS_BIGENDIAN)


AC_DISABLE_STATIC
LT_INIT(dlopen, win32-dll)
AC_SUBST(LIBTOOL_DEPS)

dnl Define c-icap version
C_ICAP_HEX_VERSION=`echo $PACKAGE_VERSION | $AWK -f $srcdir/build/c_icap_version.awk`
AC_SUBST(C_ICAP_HEX_VERSION)

dnl Checks for OS specific flags and posix threads libraries.....

case "$host_os" in
     linux*)
	CFLAGS="-D_REENTRANT $CFLAGS"
	THREADS_LDADD="-lpthread"
	THREADS_LDFLAGS=""
	LIBS_LDFLAGS=""
     ;;
     solaris2.*)
     	  CFLAGS="-D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS $CFLAGS"
          LIBS="-lsocket -lnsl -lrt $LIBS"
	  THREADS_LDADD="-lpthread"
	  THREADS_LDFLAGS=""
	  LIBS_LDFLAGS=""
      ;;
      freebsd5*)
## If I understand how all those threading models works correctly
## in FreeBSD I will make an option in configure script
## --with-freebsd-threads={c_r,pthreads,linuxthreads,thr}
## If I am correct I must compile c-icap with the way
## external libraries are compiled. (The clamav uses -lc_r and I had problems
## using a different threading model)

##    FreeBSD linuxthreads flags
#      	CFLAGS="-D_THREAD_SAFE -I/usr/local/include/pthread/linuxthreads $CFLAGS"
#	THREADS_LDADD="-llthread -lgcc_r"
#	THREADS_LDFLAGS="-L/usr/local/lib"

##    FreeBSD Standard threads
      	CFLAGS="-pthread -D_THREAD_SAFE  $CFLAGS"
	THREADS_LDADD="-XCClinker -lc_r"
	THREADS_LDFLAGS=""
	LIBS_LDFLAGS=""
## FreeBSD has pthreads rwlocks from version 3 (I think)
#	AC_DEFINE(HAVE_PTHREADS_RWLOCK,1,[Define HAVE_PTHREADS_RWLOCK if pthreads library supports rwlocks])
##   1:1 threads
#      	CFLAGS="-D_THREAD_SAFE  $CFLAGS"
#	THREADS_LDADD="-XCClinker -lthr"
#	THREADS_LDFLAGS=""

      ;;

      freebsd6*)
        CFLAGS="-D_THREAD_SAFE  $CFLAGS"
        THREADS_LDADD="-XCClinker -lthr"
        THREADS_LDFLAGS=""
	LIBS_LDFLAGS=""
      ;;

      cygwin*)
	CFLAGS="-D_REENTRANT $CFLAGS"
	THREADS_LDADD="-lpthread"
	THREADS_LDFLAGS="";
	LIBS_LDFLAGS="-no-undefined"
	iscygwin="yes"
      ;;
      *)
      	CFLAGS="-D_REENTRANT $CFLAGS"
	THREADS_LDADD="-lpthread"
	THREADS_LDFLAGS=""
	LIBS_LDFLAGS=""
      ;;
esac

TEST_LIBS="$TEST_LIBS $THREADS_LDADD"
AC_SUBST(THREADS_LDADD)
AC_SUBST(THREADS_LDFLAGS)
AC_SUBST(LIBS_LDFLAGS)

AC_DEFINE_UNQUOTED(C_ICAP_CONFIGURE_OPTIONS, "$ac_configure_args",
                   [configure command line used to configure c-icap])
AC_DEFINE_UNQUOTED(C_ICAP_CONFIG_HOST_TYPE, "$host",[Host type from configure])


CFLAGS="$CFLAGS -Wall"

AC_CACHE_CHECK([for __attribute__((visibility("default")))],
    ac_cv_default_visibility_attribute, [
    echo 'int __attribute__ ((visibility ("default"))) foo_visible (void) { return 1; } int foo_invisible(void) {return 1;}' > conftest.c
    ac_cv_default_visibility_attribute=no
    if AC_TRY_COMMAND(${CC-cc} -fvisibility=hidden -Werror -S conftest.c -o conftest.s 1>&AS_MESSAGE_LOG_FD);
    then
        if grep '\.hidden.*foo_invisible' conftest.s >/dev/null && ! grep '\.hidden.*foo_visible' conftest.s >/dev/null;
        then
            ac_cv_default_visibility_attribute=yes
        fi
# Else try to detect visibility for Sun solaris:
#    CC -xldscope={global|hidden}
# and use __global/__hidden inside C code
#   elif
    fi
    rm -f conftest.*
    ])

INVISIBILITY_CFLAG=""
VISIBILITY_ATTR="0"
if test $ac_cv_default_visibility_attribute = yes;
then
    AC_DEFINE(HAVE_VISIBILITY_ATTRIBUTE, 1,
          [Define if __attribute__((visibility("default"))) is supported.])
    INVISIBILITY_CFLAG="-fvisibility=hidden"
    VISIBILITY_ATTR="1"
fi
AC_SUBST(INVISIBILITY_CFLAG)
AC_SUBST(VISIBILITY_ATTR)

AC_MSG_CHECKING([Whether large file support should enabled])
AC_ARG_ENABLE(large_files,
[  --enable-large-files	Enable large files support],
[ if test $enableval = "yes"; then
    large_file_support="yes"
    AC_MSG_RESULT(yes)
  else
    large_file_support="no"
    AC_MSG_RESULT(no)
  fi
],
   [ large_file_support="yes"
     AC_MSG_RESULT(yes)
   ]
)

if test $large_file_support = "yes"; then
   CFLAGS="$CFLAGS -D_FILE_OFFSET_BITS=64"
#here I must put a check if the -D_FILE_OFFSET_BITS makes the off_t an 64bit integer
# and if not supported warning the user
#Possibly checks for systems which supports large files using different defines....
#later .......
fi

AC_MSG_CHECKING([Whether to enable IPv6 support])
AC_ARG_ENABLE(ipv6,
[  --disable-ipv6	Disable ipv6 support],
[ if test $enableval = "yes"; then
      ipv6_support="yes"
      AC_MSG_RESULT(yes)
  else
      ipv6_support="no"
      AC_MSG_RESULT(no)
  fi
],
   [ ipv6_support="yes"
     AC_MSG_RESULT(yes)
   ]
)

USE_IPV6="0"
if test a"$ipv6_support" = "ayes"; then
    AC_DEFINE(HAVE_IPV6,1,[Define HAVE_IPV6 if OS supports ipv6])
    USE_IPV6="1"
fi
AC_SUBST(USE_IPV6)

AC_MSG_CHECKING([Whether to use SYSV/IPC])
AC_ARG_ENABLE(sysvipc,
[  --enable-sysvipc	Enable SYSV/IPC for shared memory if supported],
[ if test $enableval = "yes"; then
    sysvipc="yes"
    AC_MSG_RESULT(yes)
  else
    sysvipc="no"
    AC_MSG_RESULT(no)
  fi
],
   [ sysvipc="yes"
     AC_MSG_RESULT(yes)
   ]
)

AC_MSG_CHECKING([Whether to use poll])
AC_ARG_ENABLE(poll,
[  --disable-poll	Disable poll(2) support],
[ if test $enableval = "no"; then
    enablepoll="no"
    AC_MSG_RESULT(no)
  else
    enablepoll="yes"
    AC_MSG_RESULT(yes)
  fi
],
   [ enablepoll="yes"
     AC_MSG_RESULT(yes)
   ]
)

USE_COMPAT="0"
AC_MSG_CHECKING([Keep library compatibility])
AC_ARG_ENABLE(lib_compat,
[  --enable-lib-compat	Enable library compatibility with older c-icap versions],
[ if test $enableval = "yes"; then
    lib_compat="yes"
    AC_MSG_RESULT(yes)
    USE_COMPAT="1"
  fi
],
   [ lib_compat="no"
     AC_MSG_RESULT(no)
    ]
)
AC_SUBST(USE_COMPAT)


# Checks for programs
AC_CHECK_PROG(has_doxygen, doxygen, "yes", "no")
if test a"$has_doxygen" = "ayes"; then
   doxygen_bin=doxygen
else
   doxygen_bin="echo Doxygen is not installed /"
fi
AC_SUBST(doxygen_bin)

# Check if we need to enable rpath when linking with libraries
AC_ARG_ENABLE(rpath,
[  --enable-rpath       hardcode runtime library paths],
[
     case "$enableval" in
     yes)
	enable_rpath="yes";
     ;;
     no)
	enable_rpath="no";
     ;;
     *)
	enable_rpath="yes";
        # Build list with libraries to use rpath
        ENABLE_RPATH_LIBS=`echo $enableval| tr ';:,' ' '`
        echo "Enable rpath for libs:"$ENABLE_RPATH_LIBS
        # search into $ENABLE_RPATH_LIBS using:
        # if test "${ENABLE_RPATH_LIBS#*zlib}" != "$ENABLE_RPATH_LIBS"; then echo found; fi
     ;;
     esac
],
   [enable_rpath="no"]
)

#Routines used for checking libraries
AC_DEFUN([ICFG_STATE_SAVE],[
#save state
ICFG_OLD_CPPFLAGS=$CPPFLAGS
ICFG_OLD_CFLAGS=$CFLAGS
ICFG_OLD_LDFLAGS=$LDFLAGS
ICFG_OLD_LIBS=$LIBS
])

#Routines used for checking libraries
AC_DEFUN([ICFG_STATE_ROLLBACK],[
#rollback saved state
CPPFLAGS=$ICFG_OLD_CPPFLAGS
CFLAGS=$ICFG_OLD_CFLAGS
LDFLAGS=$ICFG_OLD_LDFLAGS
LIBS=$ICFG_OLD_LIBS
])

AC_DEFUN([ICFG_BUILD_FLAGS_2], [
# The *_LNDIR_LDADD used by external programs to link with cicapapi libibrary.
# They must link with libraries options if the cicapapi is not linked with
# -rpath option and not standard directories are used.
# TODO: support multiple directories
if test -n $2; then
    $1_ADD_FLAG=-I$2
else
    $1_ADD_FLAG=""
fi

$1_LNDIR_LDADD=""
if test -n $3; then
    if test "a$enable_rpath" = "ayes"; then
        $1_ADD_LDADD="-Wl,-rpath -Wl,$3 -L$3 "$4
    else
        $1_ADD_LDADD="-L$3 "$4
        $1_LNDIR_LDADD="-L$3 "$4
    fi
else
    $1_ADD_LDADD=$4
fi
  AC_SUBST($1_LNDIR_LDADD)
  AC_SUBST($1_ADD_LDADD)
  AC_SUBST($1_ADD_FLAG)
]
)

AC_DEFUN([ICFG_BUILD_FLAGS], [
  if test "a$2" != "a"; then
       ICFG_BUILD_FLAGS_2($1, $2/include, $2/lib, $3)
  else
       ICFG_BUILD_FLAGS_2($1, "", "", $3)
  fi
]
)

# Checks for libraries

AC_ARG_WITH(openssl,
[  --with-openssl	Path to openssl],
[
case "$withval" in
     yes)
	openssl=yes;
     ;;
     no)
	openssl=no;
     ;;
     *)
	openssl=yes;
        opensslpath=$withval;
     ;;
     esac
],
[ openssl=yes]
)

if test "a$openssl" != "ano"; then
   ICFG_STATE_SAVE(OPENSSL)
   if test "a$opensslpath" != "a"; then
        CFLAGS="$CFLAGS -I$opensslpath/include"
        LDFLAGS="$LDFLAGS -L$opensslpath/lib"
   fi
   LIBS="-lssl -lcrypto $LIBS"
   (test -n "$opensslpath" && echo -n "checking for OpenSSL library under $opensslpath... ") || echo -n "checking for OpenSSL library... ";
   AC_LINK_IFELSE(
         [AC_LANG_SOURCE(
                 [
                   #include <openssl/ssl.h>
                   int main(int argc, char *argv[])
                   {
                      int ret = SSL_library_init();
                      return ret;
                   }
                 ])
               ],
         [openssl=yes; echo "yes";],
         [openssl=no; echo "no"]
   )
   if test "a$openssl" = "ayes"; then
      AC_DEFINE(HAVE_OPENSSL,1,[Define HAVE_OPENSSL if openssl is installed])
      ICFG_BUILD_FLAGS(OPENSSL, $opensslpath, "-lssl -lcrypto")
   fi
   ICFG_STATE_ROLLBACK
fi

USE_OPENSSL="1"
if test a"$openssl" = "ano"; then
    USE_OPENSSL="0"
fi
AC_SUBST(USE_OPENSSL)

AC_CHECK_LIB(dl,dlopen,DL_ADD_FLAG=" -ldl")
AC_SUBST(DL_ADD_FLAG)

AC_ARG_WITH(zlib,
[  --with-zlib	Path to zlib library],
[
case "$withval" in
     yes)
	zlib=yes;
     ;;
     no)
	zlib=no;
     ;;
     *)
        zlib=yes;
	zlibpath=$withval;
     ;;
     esac
],
[ zlib=yes]
)

if test "a$zlib" != "ano"; then
   ICFG_STATE_SAVE(ZLIB)
   if test "a$zlibpath" != "a"; then
        CFLAGS="$CFLAGS -I$zlib/include"
        LDFLAGS="$LDFLAGS -L$zlib/lib"
   fi
   AC_CHECK_LIB(z,inflate,[zlib=yes],[zlib=no])
   ICFG_STATE_ROLLBACK
fi
if test "a$zlib" = "ano"; then
    AC_MSG_WARN("zlib required for the c-icap's internal filetype recognizer!")
else
    AC_DEFINE(HAVE_ZLIB,1,[Define HAVE_ZLIB if zlib installed])
    ICFG_BUILD_FLAGS(ZLIB, $zlibpath, "-lz")
fi

AC_ARG_WITH(bzlib,
[  --with-bzlib	Path to bzlib library],
[
case "$withval" in
     yes)
	bzlib=yes;
     ;;
     no)
	bzlib=no;
     ;;
     *)
        bzlib=yes;
	bzlibpath=$withval;
     ;;
     esac
],
[ bzlib=yes]
)

if test "a$bzlib" != "ano"; then
   ICFG_STATE_SAVE(BZLIB)
   if test "a$bzlibpath" != "a"; then
        CFLAGS="$CFLAGS -I$bzlibpath/include"
        LDFLAGS="$LDFLAGS -L$bzlibpath/lib"
   fi
   AC_CHECK_LIB(bz2,BZ2_bzDecompressInit,[bzlib=yes],[bzlib=no])
   ICFG_STATE_ROLLBACK
fi
if test "a$bzlib" = "ano"; then
    AC_MSG_WARN("bzlib required for the c-icap's internal filetype recognizer!")
else
    AC_DEFINE(HAVE_BZLIB,1,[Define HAVE_BZLIB if bzlib installed])
    ICFG_BUILD_FLAGS(BZLIB, $bzlibpath, "-lbz2")
fi

AC_ARG_WITH(brotli,
[  --with-brotli	Path to brotli library],
[
case "$withval" in
     yes)
	brotli=yes;
     ;;
     no)
	brotli=no;
     ;;
     *)
        brotli=yes;
	brotlipath=$withval;
     ;;
     esac
],
[ brotli=yes]
)

if test "a$brotli" != "ano"; then
   ICFG_STATE_SAVE(BROTLI)
   if test "a$brotlipath" != "a"; then
        CFLAGS="$CFLAGS -I$brotlipath/include"
        LDFLAGS="$LDFLAGS -L$brotlipath/lib -lbrotlicommon"
   fi
   AC_CHECK_LIB(brotlidec,BrotliDecoderDecompressStream,[brotli=yes],[brotli=no])
   ICFG_STATE_ROLLBACK
fi
if test "a$brotli" = "ano"; then
    AC_MSG_WARN("brotli required for the c-icap's internal filetype recognizer!")
else
    AC_DEFINE(HAVE_BROTLI,1,[Define HAVE_BROTLI if brotli installed])
    ICFG_BUILD_FLAGS(BROTLI, "$brotlipath", "-lbrotlicommon -lbrotlidec -lbrotlienc")
    # fix BROTLI_LNDIR_LDADD the linker does not find brotlicommon even if
    # it is linked to libicapapi using rpath
    if test "a$brotlipath" != "a" -a "a$BROTLI_LNDIR_LDADD" = "a"; then
        BROTLI_LNDIR_LDADD="-L$brotlipath/lib -lbrotlicommon"
        AC_SUBST(BROTLI_LNDIR_LDADD)
    fi
fi

AC_ARG_WITH(zstd,
[  --with-zstd	Path to zstd library],
[
case "$withval" in
     yes)
	zstd=yes;
     ;;
     no)
	zstd=no;
     ;;
     *)
        zstd=yes;
	zstdpath=$withval;
     ;;
     esac
],
[ zstd=yes]
)

if test "a$zstd" != "ano"; then
   ICFG_STATE_SAVE(ZSTD)
   if test "a$zstdpath" != "a"; then
        CFLAGS="$CFLAGS -I$zstdpath/include"
        LDFLAGS="$LDFLAGS -L$zstdpath/lib -lzstd"
   fi
   AC_CHECK_LIB(zstd,ZSTD_compressStream2,[zstd=yes],[zstd=no])
   ICFG_STATE_ROLLBACK
fi
if test "a$zstd" = "ano"; then
    AC_MSG_WARN("libzstd required for the c-icap's internal filetype recognizer!")
else
    AC_DEFINE(HAVE_ZSTD,1,[Define HAVE_ZSTD if libzstd installed])
    ICFG_BUILD_FLAGS(ZSTD, "$zstdpath", "-lzstd")
fi

libdb="yes"
libdbpath=""
AC_ARG_WITH(bdb,
[  --with-bdb        Where to find Berkeley DB library ],
[ case "$withval" in
    yes)
         libdb="yes"
    ;;
    no )
         libdb="no"
    ;;
    * )
        libdb="yes"
        libdbpath=$withval
    ;;
  esac
],
)

if test "a$libdb" != "ano"; then
   if test "a$libdbpath" != "a"; then
     CFLAGS="-I$libdbpath/include $CFLAGS"
     LDFLAGS="-L$libdbpath/lib $LDFLAGS"
   fi

   # We are going to see if we can found a Berkeley DB located under a
   # libdbpath/include/db4x directory and use lbdbpath/lib/libdb-4.x library.

   ICFG_STATE_SAVE(BDB)
   OLD_LIBS=$LIBS
   for DBVER in "" 6 6.3 6.2 6.1 6.0 5 5.4 5.3 5.2 5.1 5.0 4 4.9 4.8 4.7 4.6 4.5 4.4 4.3 4.2; do
        if test -z $DBVER; then
           usedblib="-ldb"
           incdbdir=""
        else
           usedblib="-ldb-$DBVER"
           incdbdir=db`echo $DBVER|sed 's/\.//'`"/"
        fi
        if test -z "$libdbpath"; then
           print_libdbpath="..."
        else
            print_libdbpath="under $libdbpath..."
        fi
        echo -n "checking for BerleleyDB v$DBVER $print_libdbpath"
        LIBS="$usedblib $OLD_LIBS"
        AC_LINK_IFELSE(
               [AC_LANG_SOURCE(
                 [
                     #include <${incdbdir}db.h>
                     int main(){
                        int major,minor,patch;
                        if (!db_version(&major,&minor,&patch))
                             return -1;
                        return 0;
                     }
                  ])
               ],
               [echo yes;libdb="yes";],
               [echo "no";libdb="no";]
        )

        if test a"$libdb" = "ayes"; then
           ICFG_BUILD_FLAGS(BDB, "$libdbpath", $usedblib)
           AC_DEFINE(HAVE_BDB, 1, [Define HAVE_BDB if berkeley DB is installed])
           AC_DEFINE_UNQUOTED(BDB_HEADER_PATH(incfile), [<${incdbdir}incfile>], [Set DB_HEADER_PATH macro to compute berkeley DB header subpath])
           break;
        fi
   done

   ICFG_STATE_ROLLBACK
fi

# Detect LMDB library
liblmdb="yes"
liblmdbpath=""
AC_ARG_WITH(lmdb,
[  --with-lmdb        Where to find LMDB library ],
[ case "$withval" in
    yes)
         liblmdb="yes"
    ;;
    no )
         liblmdb="no"
    ;;
    * )
        liblmdb="yes"
        liblmdbpath=$withval
    ;;
  esac
],
)

if test a"$liblmdb" != "ano"; then
   ICFG_STATE_SAVE(LDAP)
   if test "a$liblmdbpath" != "a"; then
      CFLAGS="-I$liblmdbpath/include $CFLAGS"
      LDFLAGS="-L$liblmdbpath/lib $LDFLAGS"
   fi
   AC_CHECK_LIB(lmdb, mdb_version, [liblmdb="yes"], [liblmdb="no"])
   ICFG_STATE_ROLLBACK

   if test a"$liblmdb" = "ayes"; then
      AC_DEFINE(HAVE_LMDB, 1, [Define HAVE_LMDB if the LMDB library is installed])
      ICFG_BUILD_FLAGS(LMDB, $liblmdbpath, "-llmdb")
   fi
fi

libldap="yes"
AC_ARG_WITH(ldap,
[  --with-ldap        Where to find LDAP libraries ],
[ case "$withval" in
    yes)
         libldap="yes"
    ;;
    no )
         libldap="no"
    ;;
    * )
        libldap="yes"
        libldappath=$withval
    ;;
  esac
],
)

if test "a$libldap" != "ano"; then
   ICFG_STATE_SAVE(LDAP)
   if test "a$libldappath" != "a"; then
        CFLAGS="$CFLAGS -I$libldappath/include"
        LDFLAGS="$LDFLAGS -L$libldappath/lib"
   fi
   useldaplib=""
   AC_CHECK_LIB(ldap_r,  ldap_search_ext_s,
                [libldap="yes";useldaplib="ldap_r"],
                [libldap="no"]
                )

   if test "a$libldap" = "ano"; then
      AC_CHECK_LIB(ldap,  ldap_search_ext_s,
                   [libldap="yes";useldaplib="ldap"],
                   [libldap="no"]
                   )
   fi

   if test "a$libldap" = "ayes"; then
       AC_DEFINE(HAVE_LDAP, 1, [Define HAVE_LDAP if LDAP libraries are installed])
       ICFG_BUILD_FLAGS(LDAP, "$libldappath", "-l$useldaplib -llber")
   fi
   ICFG_STATE_ROLLBACK
fi

# Detect memcached library
libmemcached="yes"
libmemcachedpath=""
AC_ARG_WITH(memcached,
[  --with-memcached        Where to find Memcached library ],
[ case "$withval" in
    yes)
         libmemcached="yes"
    ;;
    no )
        libmemcached="no"
    ;;
    * )
        libmemcachedpath=$withval
        libmemcached="yes"
    ;;
  esac
],
)

if test a"$libmemcached" != "ano"; then
    ICFG_STATE_SAVE(MEMCACHED)
    if test a"$libmemcachedpath" != "a"; then
       CFLAGS="-l$libmemcachedpath/include $CFLAGS"
    fi
    AC_CHECK_HEADERS(libmemcached/memcached.h, [libmemcached="yes"],[libmemcached="no"])
    if test "a$libmemcached" = "ayes"; then
       ICFG_BUILD_FLAGS(MEMCACHED, "$libmemcachedpath", "-lmemcached -lmemcachedutil")
    fi
    ICFG_STATE_ROLLBACK
fi

# Check for PCRE2 regex library
AC_ARG_WITH(pcre2,
[       --with-pcre2     Path to PCRE2 library],
[
case "$withval" in
     yes)
        pcre2=yes;
     ;;
     no)
        pcre2=no;
     ;;
     *)
        pcre2=yes;
        pcre2path=$withval;
     ;;
     esac
],
[ pcre2=yes]
)

if test a"$pcre2" != "ano"; then
   ICFG_STATE_SAVE(PCRE2)
   CPPFLAGS="$CPPFLAGS -DPCRE2_CODE_UNIT_WIDTH=8"
   if test "a$pcre2path" != "a"; then
       CFLAGS="$CFLAGS -I$pcre2path/include"
       LDFLAGS="$LDFLAGS -L$pcre2path/lib"
   fi
   AC_CHECK_HEADERS(pcre2.h,
                    AC_CHECK_LIB(pcre2-8, pcre2_match_8,[pcre2=yes],[pcre2=no]),
                    [pcre2=no]
   )

   if test "a$pcre2" = "ayes"; then
       AC_DEFINE(HAVE_PCRE2,1,[Define HAVE_PCRE2 if pcre2 installed])
       ICFG_BUILD_FLAGS(PCRE2, "$pcre2path", "-lpcre2-8")
   fi
   ICFG_STATE_ROLLBACK
fi

# Check for PCRE regex library
AC_ARG_WITH(pcre,
[       --with-pcre     Path to PCRE library],
[
case "$withval" in
     yes)
        pcre=yes;
     ;;
     no)
        pcre=no;
     ;;
     *)
        pcre=yes;
        pcrepath=$withval;
     ;;
     esac
],
[ pcre=yes]
)

if test a"$pcre" != "ano" -a a"$pcre2" != "ayes"; then
   ICFG_STATE_SAVE(PCRE)
   if test "a$pcrepath" != "a"; then
       CFLAGS="$CFLAGS -I$pcrepath/include"
       LDFLAGS="$LDFLAGS -L$pcrepath/lib"
   fi
   AC_CHECK_HEADERS(pcre.h,
                    AC_CHECK_LIB(pcre, pcre_exec,[pcre=yes],[pcre=no]),
                    [pcre=no]
   )

   if test "a$pcre" = "ayes"; then
       AC_DEFINE(HAVE_PCRE,1,[Define HAVE_PCRE if pcre installed])
       ICFG_BUILD_FLAGS(PCRE, "$pcrepath", "-lpcre")
   fi
   ICFG_STATE_ROLLBACK
fi

# Check for header files
AC_CHECK_HEADERS(stdio.h strings.h unistd.h sys/stat.h limits.h assert.h)

#Check for glibc versions
AC_CHECK_HEADERS(gnu/libc-version.h)

SYS_TYPES_H="0"
AC_CHECK_HEADERS(sys/types.h,
[AC_DEFINE(HAVE_SYS_TYPES_H,1,[Define HAVE_SYS_TYPES_H if you have the <sys/types.h> header file.])
SYS_TYPES_H="1"
]
)
AC_SUBST(SYS_TYPES_H)

INTTYPES_H="0"
AC_CHECK_HEADERS(inttypes.h,
[AC_DEFINE(HAVE_INTTYPES_H,1,[Define HAVE_INTTYPES_H if you have the <inttypes.h> header file.])
INTTYPES_H="1"
]
)
AC_SUBST(INTTYPES_H)

posix_regex=no
AC_CHECK_HEADERS(regex.h,
  [posix_regex=yes;AC_DEFINE(HAVE_REGEX,1,[Define HAVE_REGEX if regex.h exists (posix regular expressions - maybe more tests needed)])],
  [posix_regex=no]
  )

USE_REGEX=0
if test "a$pcre2" = "ayes" -o "a$pcre" = "ayes" -o "a$posix_regex" = "ayes"; then
   USE_REGEX=1
fi
AC_SUBST(USE_REGEX)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
DEFINE_SIZE_T="0"
AC_CHECK_TYPE(size_t,,[DEFINE_SIZE_T="1"])
AC_SUBST(DEFINE_SIZE_T)

DEFINE_OFF_T="0"
AC_CHECK_TYPE(off_t,,[DEFINE_OFF_T="1"])
AC_SUBST(DEFINE_OFF_T)

AC_CHECK_SIZEOF(off_t)
DEFINE_SIZE_OFF_T=$ac_cv_sizeof_off_t
AC_SUBST(DEFINE_SIZE_OFF_T)

AC_CHECK_SIZEOF(long)
DEFINE_SIZE_LONG=$ac_cv_sizeof_long
AC_SUBST(DEFINE_SIZE_LONG)

AC_CHECK_SIZEOF(void *)
DEFINE_SIZE_VOID_P=$ac_cv_sizeof_void_p
AC_SUBST(DEFINE_SIZE_VOID_P)

DEFINE_UINT8="0"
AC_CHECK_TYPE(uint8_t,,[DEFINE_UINT8="1"])
AC_SUBST(DEFINE_UINT8)

DEFINE_INT8="0"
AC_CHECK_TYPE(int8_t,,[DEFINE_INT8="1"])
AC_SUBST(DEFINE_INT8)

DEFINE_UINT64="0"
AC_CHECK_TYPE(uint64_t,,[DEFINE_UINT64="1"])
AC_SUBST(DEFINE_UINT64)

DEFINE_INT64="0"
AC_CHECK_TYPE(int64_t,,[DEFINE_INT64="1"])
AC_SUBST(DEFINE_INT64)

have_int128="no"
AC_CHECK_TYPE(__int128,
[have_int128="yes"]
[AC_DEFINE(HAVE_INT128,1,[Define HAVE_INT128 if compiler supports 128bit integers])]
)

#some type size (currently they are not used)
AC_CHECK_SIZEOF(short)
DEFINE_SIZE_SHORT=$ac_cv_sizeof_short
AC_CHECK_SIZEOF(int)
DEFINE_SIZE_INT=$ac_cv_sizeof_int
AC_CHECK_SIZEOF(long long)
DEFINE_SIZE_LONG_LONG=$ac_cv_sizeof_long_long

# Checks for library functions.

#Here we are changing the LIBS variable and save the current value to OLD_LIBS variable
EXTRALIBS=""
OLD_LIBS="$LIBS"
LIBS="$LIBS $TEST_LIBS"

#AC_FUNC_VPRINTF

AC_CHECK_FUNCS(nanosleep,
AC_DEFINE(HAVE_NANOSLEEP,1,[Define HAVE_NANOSLEEP if nanosleep exists])
)

AC_CHECK_FUNCS(inet_aton,
AC_DEFINE(HAVE_INET_ATON,1,[Define HAVE_INET_ATON if inet_aton exists])
)

AC_CHECK_FUNCS(strnstr,
AC_DEFINE(HAVE_STRNSTR,1,[Define HAVE_STRNSTR if strnstr exists])
)

AC_CHECK_FUNCS(strcasestr,
AC_DEFINE(HAVE_STRCASESTR,1,[Define HAVE_STRCASESTR if strcasestr exists])
)

AC_CHECK_FUNCS(strncasestr,
AC_DEFINE(HAVE_STRNCASESTR,1,[Define HAVE_STRNCASESTR if strncasestr exists])
)

AC_CHECK_FUNCS(setgroups)

AC_CHECK_FUNCS(inet_pton)
AC_CHECK_FUNCS(inet_ntop)
AC_CHECK_FUNCS(pread)

AC_FUNC_STRERROR_R

USE_POLL="0"
if test a"$enablepoll" != "ano"; then
AC_CHECK_HEADERS(poll.h,
    AC_CHECK_FUNCS(poll,
    USE_POLL="1"
    )
)
fi
AC_SUBST(USE_POLL)
# if test a"$USE_POLL" = "1"; then
#    AC_DEFINE(HAVE_POLL,1,[Define HAVE_POLL if poll(2) exists and we can use it])
# fi

#sysv ipc
SYSV_IPC="0"
AC_CHECK_HEADERS(sys/ipc.h,
[AC_DEFINE(HAVE_SYSV_IPC,1,[Define HAVE_SYSV_IPC if sys/ipc.h exists (maybe more tests needed)])
SYSV_IPC="1"
]
)
AC_SUBST(SYSV_IPC)

POSIX_MAPPED_FILES="0"
AC_CHECK_FUNCS(mmap munmap,
[AC_DEFINE(HAVE_POSIX_MAPPED_FILES,1,[Define HAVE_POSIX_MAPPED_FILES if mmap and munmap exists])
POSIX_MAPPED_FILES="1"
]
)
AC_SUBST(POSIX_MAPPED_FILES)

dnl Checking if union semun exists in this system.
AC_MSG_CHECKING([if union semun defined])
AC_COMPILE_IFELSE(
[AC_LANG_SOURCE([
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
int main(int argc, char *argv[])
{
   union semun a_semun;
}
]
)],
[AC_DEFINE(HAVE_UNION_SEMUN,1,[Define HAVE_UNION_SEMUN if union semun defined in ipc]) AC_MSG_RESULT(yes)],
[AC_MSG_RESULT(no)]
)

dnl Checking if interprocess posix semaphores works....
AC_CACHE_CHECK([if posix 1003.1b interprocess semaphores works], ac_cv_10031b_ipc_sem,
   [AC_RUN_IFELSE(
   [AC_LANG_SOURCE([
   #include <semaphore.h>
   #include <sys/wait.h>

   int main(int argc,char **argv){
         sem_t s;
         pid_t pid;
         int status;
         if(sem_init(&s,1,1)!=0){
              return -1;
         }
         if((pid=fork())==0){
	     if(sem_post(&s)<0){
	          exit(-1);
	     }
	     exit(0);
         } else {
	    waitpid(pid,&status,0);
	    if(WEXITSTATUS(status)!=0)
	         exit(-1);
         }
         sem_destroy(&s);
         exit(0);
   }
   ]
   )],
   [ac_cv_10031b_ipc_sem=yes],
   [ac_cv_10031b_ipc_sem=no],
   [AC_MSG_ERROR([cross-compiling,
	presetting ac_cv_10031b_ipc_sem=(yes|no) will help])]
        )
   ]
)

AS_IF(
      [test "a$ac_cv_10031b_ipc_sem" = "ayes"],
      [AC_DEFINE(HAVE_POSIX_SEMAPHORES,1,[Define HAVE_POSIX_SEMAPHORES if posix 1003.1b semaphores works])
      POSIX_SEMAPHORES="1"
      ],[
      POSIX_SEMAPHORES="0"]
)
AC_SUBST(POSIX_SEMAPHORES)

dnl Checking for file locking
AC_CACHE_CHECK([if fcntl file locking works], ac_cv_fcntl,
   [AC_RUN_IFELSE(
   [AC_LANG_SOURCE([
   #include <unistd.h>
   #include <fcntl.h>


   int main(int argc,char **argv){
     struct flock fl;
     int fd;

     fd=open("autoconf.h.in",O_RDWR);

     fl.l_type=F_WRLCK;
     fl.l_whence=SEEK_SET;
     fl.l_start=0;
     fl.l_len=0;

     if(fcntl(fd,F_SETLKW,&fl)<0){
         close(fd);
          return -1;
     }
     close(fd);
     return 0;
   }
   ])],
   [ac_cv_fcntl=yes],
   [ac_cv_fcntl=no],
   [AC_MSG_ERROR([cross-compiling,
	presetting ac_cv_fcntl=(yes|no) will help])]
        )
   ]
)

AS_IF(
        [test "a$ac_cv_fcntl" = "ayes"],
        [AC_DEFINE(HAVE_POSIX_FILE_LOCK,1,[Define HAVE_POSIX_FILE_LOCK if posix fcntl file locking works])
        POSIX_FILE_LOCK="1"
        ],[
        POSIX_FILE_LOCK="0"
   ]
)
AC_SUBST(POSIX_FILE_LOCK)

AC_MSG_CHECKING([if posix shared mem works])
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
[
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
  int fd = shm_open("foo", O_CREAT|O_RDWR, S_IRUSR | S_IWUSR);
  if (fd < 0)
      return 0;
  if (shm_unlink("foo") < 0)
      return 0;
}
])],
[
AC_DEFINE(HAVE_POSIX_SHARED_MEM, 1, [Define HAVE_POSIX_SHARED_MEM if shm_open/shm_unlink functions implemented])
AC_MSG_RESULT(yes)
POSIX_SHARED_MEM="1"
],
[AC_MSG_RESULT(no)
POSIX_SHARED_MEM="0"
],
)

if test a"$POSIX_SHARED_MEM" = "a1"; then
# Linux and solaris define the shm_open in -rt library.
# This library already included in LIBS for solaris
case "$host_os" in
     linux*)
        EXTRALIBS="$EXTRALIBS -lrt"
     ;;
     *)
esac
fi

AC_SUBST(POSIX_SHARED_MEM)

#pthread_rwlock
PTHREADS_RWLOCK="0"
AC_MSG_CHECKING([if have pthread_rwlock])
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
[
#include <pthread.h>
int main(int argc, char *argv[])
{
  pthread_rwlock_t lock;
}
])],
[
AC_DEFINE(HAVE_PTHREADS_RWLOCK,1,[Define HAVE_PTHREADS_RWLOCK if pthreads library supports rwlocks])
PTHREADS_RWLOCK="1"
AC_MSG_RESULT(yes)
],
AC_MSG_RESULT(no),
)
AC_SUBST(PTHREADS_RWLOCK)

# interprocess pthread mutexes
PTHREADS_PROCESS_SHARED="0"
AC_MSG_CHECKING([if have PTHREAD_PROCESS_SHARED])
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
[
#include <pthread.h>
int main(int argc, char *argv[])
{
   pthread_mutexattr_t mtx_attr;
   pthread_mutexattr_init(&mtx_attr);
   pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_SHARED);
   pthread_mutexattr_setrobust(&mtx_attr, PTHREAD_MUTEX_ROBUST);
}
])],
[
AC_DEFINE(HAVE_PTHREADS_PROCESS_SHARED,1,[Define HAVE_PTHREADS_PROCESS_SHARED if pthreads library supports interprocess shared mutexes])
PTHREADS_PROCESS_SHARED="1"
AC_MSG_RESULT(yes)
],
AC_MSG_RESULT(no),
)
AC_SUBST(PTHREADS_PROCESS_SHARED)

# check for ioctl/TIOCGWINZ/winsize
AC_MSG_CHECKING([ioctl supports TIOCGWINSZ/winsize])
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
[
#include <sys/ioctl.h>
int main(int argc, char *argv[])
{
 struct winsize ws;
 return ioctl(0, TIOCGWINSZ, &ws);
}
])],
[
AC_DEFINE(HAVE_TIOCGWINSZ,1,[Define HAVE_TIOCGWINSZ if ioctl supports TIOCGWINSZ/winsize])
AC_MSG_RESULT(yes)
],
AC_MSG_RESULT(no),
)

C11_ATOMICS="0"
AC_MSG_CHECKING([if have c11 atomics])
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
[
#include <stdatomic.h>
int main(int argc, char *argv[])
{
   long long d = 0;
   atomic_fetch_add(&d, 1);
}
])],
[
AC_DEFINE(HAVE_C11_ATOMICS,1,[Define HAVE_C11_ATOMICS if c11 atomics supported])
C11_ATOMICS="1"
AC_MSG_RESULT(yes)
],
AC_MSG_RESULT(no)
)

AC_MSG_CHECKING([if have 128bit atomics])
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
[
#include <stdatomic.h>
int main(int argc, char *argv[])
{
   __int128 d = 0;
   atomic_fetch_add(&d, 1);
}
])],
[
AC_DEFINE(HAVE_128BIT_ATOMICS,1,[Define HAVE_128BIT_ATOMICS if 128bit atomics supported])
AC_MSG_RESULT(yes)
],
AC_MSG_RESULT(no)
)

if test a"$C11_ATOMICS" = "a1"; then
   # We can not safely use AC_CHECK_LIB for libatomic:
   # This library may implement zero or all of the
   # atomic_* operators/functions depending the
   # architecture and the AC_CHECK_LIB requires one
   # function exist in checking library.
   # For now try to check if the libatomic exist,
   # and if yes use it,  without checking if really
   # needed by c-icap libraries and executables
   AC_MSG_CHECKING([if needs linking with libatomic])
   ICFG_STATE_SAVE(ATOMIS)
   LIBS="-latomic"
   AC_LINK_IFELSE(
   [
   AC_LANG_SOURCE(
        [#include <stdatomic.h>
        int main(int argc, char *argv[])
        {
           long long d;
           atomic_fetch_add(&d, 1);
        }]
   )],
   NEEDS_ATOMIC_LIB="1"
   AC_MSG_RESULT(yes),
   AC_MSG_RESULT(no))
fi

if test a"$NEEDS_ATOMIC_LIB" = "a1"; then
  EXTRALIBS="$EXTRALIBS -latomic"
fi

# libicapapi requires linking with math library
EXTRALIBS="$EXTRALIBS -lm"

ICFG_STATE_ROLLBACK

#We are pouting back real LIBS variable
LIBS=$OLD_LIBS
LIBS="$LIBS $EXTRALIBS"

#Configure common flags
if test a"$iscygwin" != "a"; then
MODULES_LIBADD="-lcicap -licapapi"
else
MODULES_LIBADD=""
fi
MODULES_CFLAGS="$INVISIBILITY_CFLAG -DCI_BUILD_MODULE"
AC_SUBST(MODULES_LIBADD)
AC_SUBST(MODULES_CFLAGS)

#general parameters
AM_CONDITIONAL(ISCYGWIN,[test a"$iscygwin" != a])

# Now determine which modules will going to build .....

AM_CONDITIONAL(USE_CPLUSPLUS, [test "a$CXX" != "a"])
AM_CONDITIONAL(USE_OPENSSL, [test a"$openssl" != "ano"])
AM_CONDITIONAL(USE_PCRE2, [test a"$pcre2" = "ayes"])
AM_CONDITIONAL(USE_PCRE, [test a"$pcre" = "ayes"])
AM_CONDITIONAL(USE_REGEX, [test a"$pcre2" = "ayes" -o a"$pcre" = "ayes" -o a"$posix_regex" = "ayes"])
AM_CONDITIONAL(USEBDB,   [test a"$libdb" != ano])
AM_CONDITIONAL(USELMDB,   [test a"$liblmdb" != ano])
AM_CONDITIONAL(USELDAP, [test a"$libldap" != ano])
AM_CONDITIONAL(USEMEMCACHED, [test a"$libmemcached" != ano])
AM_CONDITIONAL(USE_RPATH, [test "a$enable_rpath" != "ano"])

AC_CONFIG_FILES([
include/c-icap-conf.h include/c-icap-conf-w32.h Makefile utils/Makefile services/Makefile
services/echo/Makefile services/ex-206/Makefile modules/Makefile tests/Makefile
docs/Makefile docs/man/Makefile
])
AC_OUTPUT
