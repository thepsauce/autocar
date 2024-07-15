#include "args.h"
#include "file.h"
#include "conf.h"
#include "salloc.h"
#include "util.h"
#include "macros.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static int set_conf(const char *name, char *value)
{
    switch (name[0]) {
    case 'b':
    case 'B':
        if (strcasecmp(&name[1], "uild") != 0) {
            return 1;
        }
        Config.build = get_relative_path(value);
        if (Config.build == NULL) {
            return -1;
        }
        break;

    case 'c':
    case 'C':
        if (tolower(name[1]) == 'c') {
            if (name[2] != '\0') {
                return 1;
            }
            Config.cc = sstrdup(value);
        } else if (name[1] == '_' || name[1] == ' ') {
            if (strcasecmp(&name[2], "flags") == 0) {
                split_string_at_space(value,
                        &Config.c_flags, &Config.num_c_flags);
                for (size_t i = 0; i < Config.num_c_flags; i++) {
                    Config.c_flags[i] = sstrdup(Config.c_flags[i]);
                }
            } else if (strcasecmp(&name[2], "libs") == 0) {
                split_string_at_space(value,
                        &Config.c_libs, &Config.num_c_libs);
                for (size_t i = 0; i < Config.num_c_libs; i++) {
                    Config.c_libs[i] = sstrdup(Config.c_libs[i]);
                }
            } else {
                return 1;
            }
        }
        break;

    case 'd':
    case 'D':
        if (strcasecmp(&name[1], "iff") != 0) {
            return 1;
        }
        Config.diff = sstrdup(value);
        break;

    case 'e':
    case 'E':
        if (tolower(name[1]) != 'x' ||
                tolower(name[2]) != 't' || name[3] != '_') {
            return 1;
        }
        if (strcasecmp(&name[4], "source") == 0) {
            Config.exts[EXT_TYPE_SOURCE] = sstrdup(value);
        } else if (strcasecmp(&name[4], "header") == 0) {
            Config.exts[EXT_TYPE_HEADER] = sstrdup(value);
        } else if (strcasecmp(&name[4], "build") == 0) {
            Config.exts[EXT_TYPE_OBJECT] = sstrdup(value);
        } else {
            return 1;
        }
        break;

    case 'i':
    case 'I':
        if (tolower(name[1]) != 'n') {
            return 1;
        }
        if (strcasecmp(&name[2], "it") == 0) {
            Config.init = sstrdup(value);
        } else if (strcasecmp(&name[2], "terval") == 0) {
            Config.interval = strtol(value, NULL, 0);
        }
        break;

    case 'p':
    case 'P':
        if (strcasecmp(&name[1], "rompt") != 0) {
            return 1;
        }
        Config.prompt = sstrdup(value);
        break;
    }
    return 0;
}

/**
 * Parses a config file in the format:
 * <Variable> = <Value>
 * .
 * .
 * .
 * It uses `set_conf()` to set valid variable names, invalid names that do not
 * exist in the config are ignored.
 */
static bool parse_config(FILE *fp)
{
    char *line = NULL;
    size_t n;
    ssize_t l;
    char *s, *e, *equ;

    while ((l = getline(&line, &n, fp)) > 0) {
        s = line;
        while (isspace(s[0])) {
            s++;
        }
        if (line[l - 1] == '\n') {
            l--;
            line[l] = '\0';
        }
        if (s[0] == '\0') {
            continue;
        }

        equ = strchr(s, '=');
        if (equ == NULL) {
            fprintf(stderr, "invalid line: %s\n", s);
            free(line);
            return false;
        }

        e = equ;
        while (e != s) {
            e--;
            if (!isspace(e[0])) {
                break;
            }
        }
        if (e == s) {
            continue;
        }
        e[1] = '\0';

        equ++;

        if (set_conf(s, equ) == -1) {
            free(line);
            return false;
        }
    }
    free(line);
    if (errno != 0) {
        fprintf(stderr, "getline: %s\n", strerror(errno));
        return false;
    }
    return true;
}

static char *get_shebang_cmd(FILE *fp, const char *cat)
{
    char *cmd = NULL;
    size_t n = 0;
    ssize_t l;

    l = getline(&cmd, &n, fp);
    if (l < 0) {
        if (errno == 0) {
            fprintf(stderr, "invalid shebang\n");
        } else {
            fprintf(stderr, "getline: %s\n", strerror(errno));
        }
        fclose(fp);
        return false;
    }
    if (cmd[l - 1] == '\n') {
        cmd[--l] = ' ';
    } else {
        cmd[l] = ' ';
    }
    fclose(fp);

    cmd = srealloc(cmd, l + 1 + strlen(cat) + 1);
    strcat(cmd, cat);
    return cmd;
}

bool source_config(const char *conf)
{
    bool r;
    FILE *fp, *pp;
    char *cmd;

    DLOG("source: %s\n", conf);

    fp = fopen(conf, "r");
    if (fp == NULL) {
        fprintf(stderr, "fopen '%s': %s\n", conf, strerror(errno));
        return false;
    }

    if (fgetc(fp) == '#' && fgetc(fp) == '!') {
        cmd = get_shebang_cmd(fp, conf);
        if (cmd == NULL) {
            return false;
        }
        DLOG("has shebang: %s\n", cmd);
        pp = popen(cmd, "r");
        if (pp == NULL) {
            fprintf(stderr, "popen '%s': %s\n", cmd, strerror(errno));
            fclose(fp);
            free(cmd);
            return false;
        }
        free(cmd);
        r = parse_config(pp);
        pclose(pp);
    } else {
        r = parse_config(fp);
        fclose(fp);
    }
    return r;
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
