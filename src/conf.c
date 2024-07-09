#include "args.h"
#include "conf.h"
#include "salloc.h"
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

static char *get_relative_path(const char *path)
{
    char *cwd;
    size_t cwd_len;
    char *s;
    size_t index = 0;
    size_t pref_cwd = 0, pref_path = 0;
    size_t num_slashes = 0;

    cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        fprintf(stderr, "getcwd: %s\n", strerror(errno));
        exit(1);
    }
    cwd_len = strlen(cwd);

    if (path[0] == '/') {
        while (cwd[pref_cwd] == path[pref_path]) {
            /* collapse multiple '/////' into a single one */
            if (path[pref_path] == '/') {
                do {
                    pref_path++;
                } while (path[pref_path] == '/');
            } else {
                pref_path++;
            }
            pref_cwd++;
        }

        if (cwd[pref_cwd] != '\0' ||
                (path[pref_path] != '/' &&
                 path[pref_path] != '\0')) {
            /* position right at the previous slash */
            while (pref_cwd > 0 && cwd[pref_cwd] != '/') {
                pref_cwd--;
                pref_path--;
            }
        }
        pref_path++;

        for (size_t i = pref_cwd; i < cwd_len; i++) {
            if (cwd[i] == '/') {
                if (!Args.allow_parent_paths) {
                    fprintf(stderr, "'%s': path is not allowed to be"
                            " in a parent directory\n", path);
                    free(cwd);
                    return NULL;
                }
                num_slashes++;
            }
        }
    }
    /* else the path is already relative to the current path */

    s = smalloc(strlen(path) + 1);
    for (path += pref_path;; path++) {
        if (path[0] == '/') {
            do {
                path++;
            } while (path[0] == '/');
            if (path[0] == '\0') {
                break;
            }
            s[index++] = '/';
        }
        if (path[0] == '\0') {
            break;
        }
        s[index++] = path[0];
    }
    s[index] = '\0';
    free(cwd);
    return s;
}

static void split_flags(const char *flags, char ***psplit, size_t *pnum)
{
    const char *s, *e;
    char **split = NULL;
    size_t num = 0;

    s = flags;
    while (1) {
        while (isspace(s[0])) {
            s++;
        }
        e = s;
        while (!isspace(e[0]) && e[0] != '\0') {
            e++;
        }

        if (s == e) {
            break;
        }

        split = sreallocarray(split, num + 1, sizeof(*split));
        split[num++] = strndup(s, e - s);

        s = e;
    }

    *psplit = split;
    *pnum = num;
}

static int set_conf(const char *name, const char *value)
{
    char **rel = NULL;

    switch (name[0]) {
    case 'c':
    case 'C':
        if (name[1] == 'c' || name[1] == 'C') {
            if (name[2] != '\0') {
                return 1;
            }
            Config.cc = sstrdup(value);
        } else if (name[1] == '_' || name[1] == ' ') {
            if (strcasecmp(&name[2], "flags") == 0) {
                split_flags(value, &Config.c_flags, &Config.num_c_flags);
            } else if (strcasecmp(&name[2], "libs") == 0) {
                split_flags(value, &Config.c_libs, &Config.num_c_libs);
            } else {
                return 1;
            }
        }
        break;

    case 's':
    case 'S':
        if (strcasecmp(&name[1], "ources") != 0) {
            return 1;
        }
        rel = &Config.sources;
        break;

    case 't':
    case 'T':
        if (strcasecmp(&name[1], "ests") != 0) {
            return 1;
        }
        rel = &Config.tests;
        break;

    case 'b':
    case 'B':
        if (strcasecmp(&name[1], "uild") != 0) {
            return 1;
        }
        rel = &Config.build;
        break;

    case 'i':
    case 'I':
        if (strcasecmp(&name[1], "nterval") != 0) {
            return 1;
        }
        Config.interval = strtol(value, NULL, 0);
        break;
    }
    if (rel != NULL) {
        *rel = get_relative_path(value);
        if (*rel == NULL) {
            return -1;
        }
    }
    return 0;
}

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
        while (l > 0 && isspace(line[l - 1])) {
            l--;
        }
        line[l] = '\0';
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
        while (isspace(equ[0])) {
            equ++;
        }

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
    struct stat st;
    const char *check[3];

    if (Config.cc == NULL) {
        Config.cc = sstrdup("gcc");
    }
    if (Config.c_flags == NULL) {
        Config.c_flags = sstrdup("-g -fsanitize=address -Wall -Wextra -Werror");
    }
    if (Config.sources == NULL) {
        Config.sources = sstrdup("src");
    }
    if (Config.tests == NULL) {
        Config.tests = sstrdup("tests");
    }
    if (Config.build == NULL) {
        Config.build = sstrdup("build");
    }
    if (Config.interval < 0) {
        fprintf(stderr, "invalid interval value: %ld\n",
                Config.interval);
        return false;
    }
    if (Config.interval == 0) {
        Config.interval = 100;
    }

    check[0] = Config.sources;
    check[1] = Config.tests;
    check[2] = Config.build;
    for (size_t i = 0; i < ARRAY_SIZE(check); i++) {
        if (stat(check[i], &st) != 0) {
            if (errno == ENOENT) {
                if (mkdir(check[i], 0755) == -1) {
                    fprintf(stderr, "mkdir '%s': %s\n",
                            check[i], strerror(errno));
                    return false;
                }
                continue;
            }
            fprintf(stderr, "stat '%s': %s\n",
                    check[i], strerror(errno));
            return false;
        }
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "'%s' must be a directory\n",
                    check[i]);
            return false;
        }
    }
    return true;
}
