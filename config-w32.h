
/*
 Config.h file for win32 using MSVC compiler

 */
#include <time.h> /*For struct tm declaration ....*/
#define HAVE_MALLOC_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1

/*Some functions definitions */
#define snprintf _snprintf
#define strtoll  strtol

int strncasecmp(const char *s1, const char *s2, size_t n);
char *asctime_r(const struct tm *ptm, char *buffer);


/* Name of package */
#define PACKAGE "c_icap"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1.0"

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */
