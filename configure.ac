dnl Process this file with autoconf to produce a configure script.


AC_INIT(c_icap,m4_normalize(m4_include([VERSION.m4])))
dnl AC_INIT(c_icap,030606rc1)
CICAPLIB_VERSION=0:1:0                                            
AC_SUBST(CICAPLIB_VERSION)                                        
dnl CICAPLIB_VERSION is the libtool current[:revision[:age]] version info
dnl libtool directions about version info                                
dnl - library source code has changed since the last update c:r:a => c:r+1:a
dnl - interfaces have been added, removed, or changed c:r:a => c+1:0:a
dnl - interfaces have been added c:r:a => c:r:a+1
dnl - interfaces have been removed  c:r:a => c:r:0

AC_CONFIG_SRCDIR(aserver.c)
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(autoconf.h)

AM_INIT_AUTOMAKE([subdir-objects])

AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_C_O
AC_C_BIGENDIAN

AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

dnl Define c-icap version
C_ICAP_HEX_VERSION=`echo $PACKAGE_VERSION | $AWK -f $srcdir/build/c_icap_version.awk`
AC_SUBST(C_ICAP_HEX_VERSION)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

dnl Checks for OS specific flags and posix threads libraries.....

case "$target_os" in
     linux*)
	CFLAGS="-D_REENTRANT $CFLAGS"
	THREADS_LDADD="-lpthread"
	THREADS_LDFLAGS=""
     ;;
     solaris2.*)
     	  CFLAGS="-D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS $CFLAGS"
          LIBS="-lsocket -lnsl -lrt $LIBS"
	  THREADS_LDADD="-lpthread"
	  THREADS_LDFLAGS=""
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
      ;;

      cygwin*)
	CFLAGS="-D_REENTRANT $CFLAGS"
	THREADS_LDADD="-lpthread"
	THREADS_LDFLAGS="";
	iscygwin="yes"
      ;;
      *)
      	CFLAGS="-D_REENTRANT $CFLAGS"
	THREADS_LDADD="-lpthread"
	THREADS_LDFLAGS=""
      ;;
esac

TEST_LIBS="$TEST_LIBS $THREADS_LDADD"
AC_SUBST(THREADS_LDADD)
AC_SUBST(THREADS_LDFLAGS)

if test "x$GCC" != "x" ; then
        AC_USE_SYSTEM_EXTENSIONS
fi

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

AC_ARG_ENABLE(large_files,
[  --enable-large-files	Enable large files support],
[ if test $enableval = "yes"; then
    large_file_support="yes"
  else
    large_file_support="no"
  fi
],
   [ large_file_support="yes" ]
)

echo "checking whether large file support should enabled:"$large_file_support
if test $large_file_support = "yes"; then
   CFLAGS="$CFLAGS -D_FILE_OFFSET_BITS=64"
#here I must put a check if the -D_FILE_OFFSET_BITS makes the off_t  an 64bit integer
# and if not supported warning the user
#Possibly checks for systems which supports large files using different defines....
#later .......
fi

USE_IPV6="0"
AC_ARG_ENABLE(ipv6,
[  --enable-ipv6	Enable ipv6 support],
[ if test $enableval = "yes"; then
    ipv6_support="yes"
    AC_DEFINE(HAVE_IPV6,1,[Define HAVE_IPV6 if OS supports ipv6])
    USE_IPV6="1"
  fi
],
   [ ipv6_support="no" ]
)
AC_SUBST(USE_IPV6)

AC_ARG_ENABLE(sysvipc,
[  --enable-sysvipc	Enable SYSV/IPC for shared memory if supported],
[ if test $enableval = "yes"; then
    sysvipc="yes"
  else
    sysvipc="no"
  fi
],
   [ sysvipc="yes" ]
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

# Checks for libraries

AC_ARG_WITH(perl, 
[  --with-perl		Path to perl binary],
[
case "$withval" in
  yes)
       perlbin="perl"
  ;;			     
  no )
       perlbin="";
       perlcore="";
  ;;
  * )
      perlbin=$withval
  ;;
  esac
],
[
   perlbin="";
   perlcore="";
]
)

if test a"$perlbin" != a; then 
	perlcore=`$perlbin -MConfig -e 'print $Config{archlib}'`/CORE;
        perllib=`$perlbin -MConfig -e 'print $Config{libs}'`;
        perlccflags=`$perlbin -MConfig -e 'print $Config{ccflags}'`;
	perlldflags=`$perlbin -MConfig -e 'print $Config{ccdlflags}'`;
fi



AC_SUBST(perlcore)
AC_SUBST(perllib)
AC_SUBST(perlccflags)
AC_SUBST(perlldflags)


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
	zlib=$withval;
     ;;
     esac
],
[ zlib=yes]
)

if test a"$zlib" != "ano"; then
   if test a"$zlib" != "ayes"; then
        OLDCFLAGS=$CFLAGS
        OLDLDFLAGS=$LDFLAGS
        CFLAGS="$CFLAGS -I$zlib/include"
        LDFLAGS="$LDFLAGS -L$zlib/lib"
	AC_CHECK_LIB(z,inflate,[ZLIB_ADD_LDADD="-L$zlib/lib -lz";ZLIB_ADD_FLAG="-I$zlib/include";AC_DEFINE(HAVE_ZLIB,1,[Define HAVE_ZLIB if zlib installed])],[zlib=no])
        CFLAGS=$OLDCFLAGS
        LDFLAGS=$OLDLDFLAGS
	AC_SUBST(ZLIB_ADD_LDADD)
	AC_SUBST(ZLIB_ADD_FLAG)	
   else
	AC_CHECK_LIB(z,inflate,[ZLIB_ADD_LDADD=" -lz";AC_DEFINE(HAVE_ZLIB,1,[Define HAVE_ZLIB if zlib installed])],[zlib=no])
	AC_SUBST(ZLIB_ADD_LDADD)
   fi
fi
if test a"$zlib" = "ano"; then
    AC_MSG_WARN("zlib required for the c-icap's internal filetype recognizer!")
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
	bzlib=$withval;
     ;;
     esac
],
[ bzlib=yes]
)

if test a"$bzlib" != "ano"; then
   if test a"$bzlib" != "ayes"; then
        OLDCFLAGS=$CFLAGS
        OLDLDFLAGS=$LDFLAGS
        CFLAGS="$CFLAGS -I$bzlib/include"
        LDFLAGS="$LDFLAGS -L$bzlib/lib"
	AC_CHECK_LIB(bz2,BZ2_bzDecompressInit,[BZLIB_ADD_LDADD="-L$bzlib/lib -lbz2";BZLIB_ADD_FLAG="-I$bzlib/include";AC_DEFINE(HAVE_BZLIB,1,[Define HAVE_BZLIB if bzlib installed])],[bzlib=no])
        CFLAGS=$OLDCFLAGS
        LDFLAGS=$OLDLDFLAGS
	AC_SUBST(BZLIB_ADD_LDADD)
	AC_SUBST(BZLIB_ADD_FLAG)	
   else
	AC_CHECK_LIB(bz2,BZ2_bzDecompressInit,[BZLIB_ADD_LDADD=" -lbz2";AC_DEFINE(HAVE_BZLIB,1,[Define HAVE_BZLIB if bzlib installed])],[bzlib=no])
	AC_SUBST(BZLIB_ADD_LDADD)
   fi
fi
if test a"$bzlib" = "ano"; then
    AC_MSG_WARN("bzlib required for the c-icap's internal filetype recognizer!")
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

if test a"$libdb" != "ano"; then
   if test a"$libdbpath" = "a"; then
#  Save old LDFLAGS and LIBS
      OLDLDFLAGS=$LDFLAGS
      LDFLAGS="$LDFLAGS"
      OLDLIBS=$LIBS
      LIBS="-ldb $OLDLIBS"
      echo -n "checking for Berkeley DB under standard directories... "
      AC_LINK_IFELSE(
                     [
                         #include <db.h>
                         int main(){
                              int major,minor,patch;
                              if (!db_version(&major,&minor,&patch))
                                   return -1;
                              return 0;
                         }
                     ],	
	    	     [BDB_ADD_LDADD=" -ldb"; libdb="yes"; echo "yes"],
                     [libdb="no"; echo "no"]
       )
      AC_SUBST(BDB_ADD_LDADD)
#restore LDFLAGS
      LDFLAGS=$OLDLDFLAGS
      LIBS=$OLDLIBS
   else
#  Save old CFLAGS/LDFLAGS/LIBS
      OLDCFLAGS=$CFLAGS
      OLDLDFLAGS=$LDFLAGS
      OLDLIBS=$LIBS
      CFLAGS="-I$libdbpath/include $CFLAGS"
      LDFLAGS="-L$libdbpath/lib $LDFLAGS"
      LIBS="-ldb $OLDLIBS"
      echo -n "checking for Berkeley DB under $libdbpath... "
      AC_LINK_IFELSE(
                [
                    #include <db.h>
                    int main(){
                        int major,minor,patch;
                        if (!db_version(&major,&minor,&patch))
                             return -1;
                         return 0;
                    }
                ],
               [BDB_ADD_LDADD="-L$libdbpath/lib -ldb"; BDB_ADD_FLAG="-I$libdbpath/include";libdb="yes"; echo "yes"], 
               [libdb="no"; echo "no"]
      )
#restore CFLAGS/LDFLAGS
     CFLAGS=$OLDCFLAGS
     LDFLAGS=$OLDLDFLAGS
     LIBS=$OLDLIBS
   fi

   if test a"$libdb" = "ano"; then
# Here we still was not able to find a working Berkeley DB library.
# We are going to see if we can found a Berkeley DB located under a
# libdbpath/include/db4x and lbdbpath/lib/db4x directories.

     if test a"$libdbpath" = "a"; then
# if libdbpath is not given try with libdbpat="/usr"
        libdbpath="/usr"
     fi
#  Save old CFLAGS/LDFLAGS
     OLDCFLAGS=$CFLAGS
     OLDLDFLAGS=$LDFLAGS
     OLDLIBS=$LIBS
     LIBS="-ldb $OLDLIBS"
     for DBVER in 54 53 52 51 50 49 48 47 46 45 44 43 42; do
        if test -d $libdbpath/include/db$DBVER; then
           echo -n "checking for Berleley DB under $libdbpath/include/db$DBVER and $libdbpath/lib/db$DBVER... "
           CFLAGS="-I$libdbpath/include/db$DBVER $OLDCFLAGS"
           LDFLAGS="-L$libdbpath/lib/db$DBVER $OLDLDFLAGS" 
           AC_LINK_IFELSE(
                  [
                     #include <db.h>
                     int main(){
                        int major,minor,patch;
                        if (!db_version(&major,&minor,&patch))
                             return -1;
                        return 0;
                     }
                  ],
                  [echo yes;BDB_ADD_LDADD="-L$libdbpath/lib/db$DBVER -ldb"; BDB_ADD_FLAG="-I$libdbpath/include/db$DBVER";libdb="yes";], 
                  [libdb="no"; echo "no";]
           )
        fi
        if test a"$libdb" = "ayes"; then
           break;
        fi
     done
#restore CFLAGS/LDFLAGS
     CFLAGS=$OLDCFLAGS
     LDFLAGS=$OLDLDFLAGS
     LIBS=$OLDLIBS
   fi
   AC_SUBST(BDB_ADD_LDADD)
   AC_SUBST(BDB_ADD_FLAG)
fi

if test a"$libdb" = "ayes"; then
    AC_DEFINE(HAVE_BDB, 1, [Define HAVE_BDB if berkeley DB is installed])
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
        libldap=$withval
    ;;
  esac
],
)

if test a"$libldap" != "ano"; then
   if test a"$libldap" != "ayes"; then
        OLDCFLAGS=$CFLAGS
        OLDLDFLAGS=$LDFLAGS
        CFLAGS="$CFLAGS -I$libldap/include"
        LDFLAGS="$LDFLAGS -L$libldap/lib" 
        AC_CHECK_LIB(ldap,  ldap_search_ext_s,
                     [LDAP_ADD_LDADD="-L$libldap/lib -lldap"; LDAP_ADD_FLAG="-I$libldap/include"; AC_DEFINE(HAVE_LDAP, 1, [Define HAVE_LDAP if LDAP libraries are installed])],
                     [libldap="no"]
                     )
        CFLAGS=$OLDCFLAGS
        LDFLAGS=$OLDLDFLAGS
        AC_SUBST(LDAP_ADD_LDADD)
        AC_SUBST(LDAP_ADD_FLAG)
   else
        AC_CHECK_LIB(ldap,  ldap_search_ext_s,
                     [LDAP_ADD_LDADD="-lldap"; AC_DEFINE(HAVE_LDAP, 1, [Define HAVE_LDAP if LDAP libraries are installed])],
                     [libldap="no"]
                     )
        AC_SUBST(LDAP_ADD_LDADD)
   fi
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
    OLDCPPFLAGS=$CPPFLAGS
    OLDCXXFLAGS=$CXXFLAGS
    if test a"$libmemcachedpath" != "a"; then
       CPPFLAGS="-l$libmemcachedpath/include $CPPFLAGS"
       CXXFLAGS="-l$libmemcachedpath/include $CXXFLAGS"
       MEMCACHED_LDFLAGS="-L$libmemcachedpath/lib"
       MEMCACHED_CFLAGS="-I$libmemcachedpath/include"
    fi

    AC_CHECK_HEADERS(libmemcached/memcached.h, [libmemcached="yes"],[libmemcached="no"])
    CPPFLAGS=$OLDCPPFLAGS
    CXXFLAGS=$OLDCXXFLAGS
fi

AC_SUBST(MEMCACHED_LDFLAGS)
AC_SUBST(MEMCACHED_CFLAGS)


# Check for header files
AC_HEADER_STDC
AC_CHECK_HEADERS(strings.h unistd.h sys/stat.h)
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
        pcre=$withval;
     ;;
     esac
],
[ pcre=yes]
)

if test a"$pcre" != "ano"; then
   if test a"$pcre" != "ayes"; then
        OLDCFLAGS=$CFLAGS
        OLDLDFLAGS=$LDFLAGS
        CFLAGS="$CFLAGS -I$pcre/include"
        LDFLAGS="$LDFLAGS -L$pcre/lib"
        AC_CHECK_HEADERS(pcre.h,
                AC_CHECK_LIB(pcre, pcre_exec,[PCRE_ADD_LDADD="-L$pcre/lib -lpcre";PCRE_ADD_FLAG="-I$pcre/include";AC_DEFINE(HAVE_PCRE,1,[Define HAVE_PCRE if pcre installed])],[pcre=no]), 
                [pcre=no]
        )
        CFLAGS=$OLDCFLAGS
        LDFLAGS=$OLDLDFLAGS
        AC_SUBST(PCRE_ADD_LDADD)
        AC_SUBST(PCRE_ADD_FLAG) 
   else
         AC_CHECK_HEADERS(pcre.h,
                AC_CHECK_LIB(pcre, pcre_exec,[PCRE_ADD_LDADD=" -lpcre";AC_DEFINE(HAVE_PCRE,1,[Define HAVE_PCRE if pcre installed])],[pcre=no]),
                [pcre=no]
         )
        AC_SUBST(PCRE_ADD_LDADD)
   fi
fi

posix_regex=no
AC_CHECK_HEADERS(regex.h,
  [posix_regex=yes;AC_DEFINE(HAVE_REGEX,1,[Define HAVE_REGEX if regex.h exists (posix regular expressions - maybe more tests needed)])],
  [posix_regex=no]
  )

USE_REGEX=0
if test a"$pcre" = "ayes" -o a"$posix_regex" = "ayes"; then
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

#some type size (currently they are not used)
AC_CHECK_SIZEOF(short)
DEFINE_SIZEOFF_SHORT=$ac_cv_sizeof_short
AC_CHECK_SIZEOF(int)
DEFINE_SIZEOFF_INT=$ac_cv_sizeof_int
AC_CHECK_SIZEOF(long)
DEFINE_SIZEOFF_LONG=$ac_cv_sizeof_long
AC_CHECK_SIZEOF(long long)
DEFINE_SIZEOFF_LONG_LONG=$ac_cv_sizeof_long_long

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
AC_TRY_COMPILE(
[
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
], 
[union semun a_semun;],
AC_DEFINE(HAVE_UNION_SEMUN,1,[Define HAVE_UNION_SEMUN if union semun defined in ipc])
AC_MSG_RESULT(yes), 
AC_MSG_RESULT(no),
)


dnl Checking if interprocess posix semaphores works....
AC_CACHE_CHECK([if posix 1003.1b interprocess semaphores works], ac_cv_10031b_ipc_sem,
   [AC_TRY_RUN([
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
   ],
   ac_cv_10031b_ipc_sem=yes,
   ac_cv_10031b_ipc_sem=no,
   [AC_MSG_ERROR([cross-compiling,
	presetting ac_cv_10031b_ipc_sem=(yes|no) will help])]
        )
   ]
)

AS_IF(
      [test $ac_cv_10031b_ipc_sem = yes],
      [AC_DEFINE(HAVE_POSIX_SEMAPHORES,1,[Define HAVE_POSIX_SEMAPHORES if posix 1003.1b semaphores works])
      POSIX_SEMAPHORES="1"
      ],[
      POSIX_SEMAPHORES="0"]
)
AC_SUBST(POSIX_SEMAPHORES)

dnl Checking for file locking
AC_CACHE_CHECK([if fcntl file locking works], ac_cv_fcntl,
   [AC_TRY_RUN([
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
   ],
   ac_cv_fcntl=yes,
   ac_cv_fcntl=no,
   [AC_MSG_ERROR([cross-compiling,
	presetting ac_cv_fcntl=(yes|no) will help])]
        )
   ]
)

AS_IF(
        [test $ac_cv_fcntl = yes],
        [AC_DEFINE(HAVE_POSIX_FILE_LOCK,1,[Define HAVE_POSIX_FILE_LOCK if posix fcntl file locking works])
        POSIX_FILE_LOCK="1"
        ],[
        POSIX_FILE_LOCK="0"
   ]
)
AC_SUBST(POSIX_FILE_LOCK)

AC_MSG_CHECKING([if posix shared mem works])
AC_TRY_COMPILE(
[
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
], 
[int fd = shm_open("foo", O_CREAT|O_RDWR, S_IRUSR | S_IWUSR);
  if (fd < 0)
      return 0;
  if (shm_unlink("foo") < 0)
      return 0;
],
AC_DEFINE(HAVE_POSIX_SHARED_MEM, 1, [Define HAVE_POSIX_SHARED_MEM if shm_open/shm_unlink functions implemented])
[AC_MSG_RESULT(yes)
POSIX_SHARED_MEM="1"
], 
[AC_MSG_RESULT(no)
POSIX_SHARED_MEM="0"
],
)
if test a"$POSIX_SHARED_MEM" = "a1"; then
# Linux and solaris define the shm_open in -rt library.
# This library already included in LIBS for solaris
case "$target_os" in
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
AC_TRY_COMPILE(
[#include <pthread.h>], 
[pthread_rwlock_t lock;],
AC_DEFINE(HAVE_PTHREADS_RWLOCK,1,[Define HAVE_PTHREADS_RWLOCK if pthreads library supports rwlocks])
PTHREADS_RWLOCK="1"
AC_MSG_RESULT(yes), 
AC_MSG_RESULT(no),
)
AC_SUBST(PTHREADS_RWLOCK)

#We are pouting back real LIBS variable
LIBS=$OLD_LIBS
LIBS="$LIBS $EXTRALIBS"

#Configure common flags
MODULES_LIBADD=""
if test a"$iscygwin" != a; then
     MODULES_LIBADD="-L../../ -licapapi"
fi
MODULES_CFLAGS="$INVISIBILITY_CFLAG -DCI_BUILD_MODULE"
AC_SUBST(MODULES_LIBADD)
AC_SUBST(MODULES_CFLAGS)

#general parameters
AM_CONDITIONAL(ISCYGWIN,[test a"$iscygwin" != a])

# Now determine which modules will going to build .....

AM_CONDITIONAL(USE_REGEX, [test a"$pcre" = "ayes" -o a"$posix_regex" = "ayes"])
AM_CONDITIONAL(USEPERL,[test a"$perlcore" != a])
AM_CONDITIONAL(USEBDB,   [test a"$libdb" != ano])
AM_CONDITIONAL(USELDAP, [test a"$libldap" != ano])
AM_CONDITIONAL(USEMEMCACHED, [test a"$libmemcached" != ano])

AC_OUTPUT([
include/c-icap-conf.h Makefile c-icap.conf utils/Makefile services/Makefile 
services/echo/Makefile services/ex-206/Makefile modules/Makefile tests/Makefile 
docs/Makefile docs/man/Makefile
])

