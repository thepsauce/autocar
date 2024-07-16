#include "args.h"
#include "cmd.h"
#include "file.h"
#include "conf.h"
#include "salloc.h"
#include "util.h"
#include "macros.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

struct config Config;

bool find_autocar_config(const char *name_or_path)
{
    struct stat st;
    /* used to store "/\0" */
    char buf[2];

    DLOG("looking for config: %s\n", name_or_path);

    if (strchr(name_or_path, '/') != NULL) {
        if (stat(name_or_path, &st) != 0) {
            fprintf(stderr, "stat %s: %s\n",
                    name_or_path, strerror(errno));
            return false;
        }
        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "path '%s' is not a file\n", name_or_path);
            return false;
        }
        return true;
    }

    while (stat(name_or_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        /* reached root */
        if (getcwd(buf, sizeof(buf)) != NULL) {
            fprintf(stderr, "could not find config file\n");
            return false;
        }
        if (chdir("..") < 0) {
            fprintf(stderr, "chdir: '%s'\n", strerror(errno));
            return false;
        }
    }
    return true;
}

int set_conf(const char *name, char **args, size_t num_args, bool append)
{
    size_t off;

#define CHECK_SINGLE_APPEND() \
    if (num_args != 1) { \
        return SET_CONF_SINGLE; \
    } \
    if (append) { \
        return SET_CONF_APPEND; \
    }

    switch (name[0]) {
    case 'b':
    case 'B':
        if (strcasecmp(&name[1], "uild") != 0) {
            return SET_CONF_EXIST;
        }
        CHECK_SINGLE_APPEND();
        free(Config.build);
        Config.build = get_relative_path(args[0]);
        if (Config.build == NULL) {
            return -1;
        }
        break;

    case 'c':
    case 'C':
        if (tolower(name[1]) == 'c') {
            if (name[2] != '\0') {
                return SET_CONF_EXIST;
            }
            CHECK_SINGLE_APPEND();
            free(Config.cc);
            Config.cc = sstrdup(args[0]);
        } else if (name[1] == '_' || name[1] == ' ') {
            if (strcasecmp(&name[2], "flags") == 0) {
                off = append ? Config.num_c_flags : 0;
                if (!append) {
                    for (size_t i = 0; i < Config.num_c_flags; i++) {
                        free(Config.c_flags[i]);
                    }
                }
                Config.c_flags = sreallocarray(Config.c_flags,
                        off + num_args, sizeof(*Config.c_flags));
                for (size_t i = 0; i < num_args; i++) {
                    Config.c_flags[off + i] = sstrdup(args[i]);
                }
                Config.num_c_flags = off + num_args;
            } else if (strcasecmp(&name[2], "libs") == 0) {
                off = append ? Config.num_c_libs : 0;
                if (!append) {
                    for (size_t i = 0; i < Config.num_c_libs; i++) {
                        free(Config.c_libs[i]);
                    }
                }
                Config.c_libs = sreallocarray(Config.c_libs,
                        off + num_args, sizeof(*Config.c_libs));
                for (size_t i = 0; i < num_args; i++) {
                    Config.c_libs[off + i] = sstrdup(args[i]);
                }
                Config.num_c_libs = off + num_args;
            } else {
                return SET_CONF_EXIST;
            }
        } else {
            return SET_CONF_EXIST;
        }
        break;

    case 'd':
    case 'D':
        if (strcasecmp(&name[1], "iff") != 0) {
            return SET_CONF_EXIST;
        }
        CHECK_SINGLE_APPEND();
        free(Config.diff);
        Config.diff = sstrdup(args[0]);
        break;

    case 'e':
    case 'E':
        if (tolower(name[1]) != 'x' ||
                tolower(name[2]) != 't' || name[3] != '_') {
            return SET_CONF_EXIST;
        }
        if (strcasecmp(&name[4], "source") == 0) {
            CHECK_SINGLE_APPEND();
            free(Config.exts[EXT_TYPE_SOURCE]);
            Config.exts[EXT_TYPE_SOURCE] = sstrdup(args[0]);
        } else if (strcasecmp(&name[4], "header") == 0) {
            CHECK_SINGLE_APPEND();
            free(Config.exts[EXT_TYPE_OBJECT]);
            Config.exts[EXT_TYPE_OBJECT] = sstrdup(args[0]);
        } else if (strcasecmp(&name[4], "build") == 0) {
            CHECK_SINGLE_APPEND();
            free(Config.exts[EXT_TYPE_OBJECT]);
            Config.exts[EXT_TYPE_OBJECT] = sstrdup(args[0]);
        } else {
            return SET_CONF_EXIST;
        }
        break;

    case 'i':
    case 'I':
        if (strcasecmp(&name[1], "nterval") != 0) {
            return SET_CONF_EXIST;
        }
        CHECK_SINGLE_APPEND();
        Config.interval = strtol(args[0], NULL, 0);
        break;

    case 'p':
    case 'P':
        if (strcasecmp(&name[1], "rompt") != 0) {
            return SET_CONF_EXIST;
        }
        CHECK_SINGLE_APPEND();
        free(Config.prompt);
        Config.prompt = sstrdup(args[0]);
        break;
    default:
        return SET_CONF_EXIST;
    }
    return 0;
#undef CHECK_SINGLE_APPEND
}

bool source_config(const char *conf)
{
    Config.path = sstrdup(conf);
    return source_file(conf) == 0;
}

bool check_config(void)
{
    static const char *default_extensions[] = {
        [EXT_TYPE_OTHER] = "",
        [EXT_TYPE_SOURCE] = ".c",
        [EXT_TYPE_HEADER] = ".h",
        [EXT_TYPE_OBJECT] = ".o",
        [EXT_TYPE_EXECUTABLE] = "",
        [EXT_TYPE_FOLDER] = "",
    };
    static const char *default_prompt = ">>> ";
    static const char *default_build = "build";

    if (Config.cc == NULL) {
        Config.cc = sstrdup("gcc");
    }
    if (Config.diff == NULL) {
        Config.diff = sstrdup("diff");
    }
    if (Config.c_flags == NULL) {
        Config.c_flags = sstrdup("-g -fsanitize=address -Wall -Wextra -Werror");
    }

    for (int i = 0; i < EXT_TYPE_MAX; i++) {
        if (Config.exts[i] == NULL) {
            Config.exts[i] = sstrdup(default_extensions[i]);
        }
    }
    if (Config.interval < 0) {
        fprintf(stderr, "invalid interval value: %ld\n", Config.interval);
        return false;
    }
    if (Config.interval == 0) {
        Config.interval = 100;
    }

    if (Config.prompt == NULL) {
        Config.prompt = sstrdup(default_prompt);
    }

    if (Config.build == NULL) {
        Config.build = sstrdup(default_build);
    }

    if (create_recursive_directory(Config.build) == -1) {
        return false;
    }
    return true;
}
