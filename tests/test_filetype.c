#include "common.h"
#include "c-icap.h"
#include "filetype.h"
#include "array.h"
#include "client.h"


int USE_DEBUG_LEVEL = -1;
char *MAGIC_DB = NULL;
ci_list_t *FILES = NULL;

static int cfg_check_files(const char *directive, const char **argv, void *setdata);

static struct ci_options_entry options[] = {
    {
        "-d", "debug_level", &USE_DEBUG_LEVEL, ci_cfg_set_int,
        "The debug level"
    },
    {
        "-m", "magic_db", &MAGIC_DB, ci_cfg_set_str,
        "The path of the magic_db to use"
    },
    {"$$", NULL, &FILES, cfg_check_files, "files to send"},
    {NULL,NULL,NULL,NULL,NULL}
};

int cfg_check_files(const char *directive, const char **argv, void *setdata)
{
    if (!FILES)
        FILES = ci_list_create(512, sizeof(char*));

    if (!FILES) {
        ci_debug_printf(1, "Error allocating memory for ci_list_t object");
        return -0;
    }

    char *f = strdup(argv[0]);
    ci_list_push_back(FILES, &f);
    return 1;
}

void log_errors(void *unused, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

int main(int argc, char *argv[])
{
    char *fname = NULL;
    ci_client_library_init();

    __log_error = (void (*)(void *, const char *, ...)) log_errors;     /*set c-icap library log  function */

    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }
    if (USE_DEBUG_LEVEL >= 0)
        CI_DEBUG_LEVEL = USE_DEBUG_LEVEL;

    struct ci_magics_db *db = NULL;
    if (MAGIC_DB)
        db = ci_magic_db_load(MAGIC_DB);

    if (!db) {
        ci_debug_printf(1, "Required a valid magics db, '%s' is given\n", MAGIC_DB ? MAGIC_DB : "none");
        exit(-1);
    }

    while(ci_list_pop(FILES, &fname)) {
        char buf[4096];
        size_t bytes;
        FILE *f;
        if ((f = fopen(fname, "r")) == NULL) {
            ci_debug_printf(1, "Can not open file '%s'! Ignore\n", fname);
            continue;
        }
        bytes = fread(buf, 1, sizeof(buf), f);
        if (bytes > 0) {
            ci_debug_printf(5, "%s: read %lu bytes\n", fname, bytes);
            char *usefname;
            usefname = strrchr(fname, '/');
            if (!usefname)
                usefname = fname;
            else
                usefname++;
            int ft = ci_magic_data_type(buf, bytes);
            if (ft >= 0) {
                const char *type = ci_magic_type_name(ft);
                assert(type);
                const char *descr = ci_magic_type_descr(ft);
                printf("%s: %s, %s\n", usefname, type, (descr ? descr : "-"));
            } else {
                printf("%s: unknown\n", usefname);
            }
        }
        fclose(f);
        free(fname);
    }
    ci_list_destroy(FILES);
    return 0;
}
