#include "args.h"
#include "cmd.h"
#include "file.h"
#include "conf.h"
#include "salloc.h"
#include "util.h"
#include "macros.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

struct config Config;

struct config_entry *get_conf(const char *name, size_t *pindex)
{
    size_t l, m, r;
    int cmp;
    struct config_entry *entry;

    l = 0;
    r = Config.num_entries;
    while (l < r) {
        m = (l + r) / 2;

        entry = &Config.entries[m];
        cmp = strcasecmp(entry->name, name);
        if (cmp == 0) {
            if (pindex != NULL) {
                *pindex = m;
            }
            return entry;
        }
        if (cmp < 0) {
            l = m + 1;
        } else {
            r = m;
        }
    }
    if (pindex != NULL) {
        *pindex = r;
    }
    return NULL;
}

/**
 * Returns the index of the given value within the entry or `SIZE_MAX` if the
 * entry was not found.
 */
static size_t get_entry_value_index(struct config_entry *ce, const char *value)
{
    for (size_t i = 0; i < ce->num_values; i++) {
        if (strcmp(ce->values[i], value) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

int set_conf(const char *name, const char **values,
        size_t num_values, int mode)
{
    size_t index;
    struct config_entry *entry;
    char *upper;
    char *env;
    size_t env_len, env_i;

    DLOG("changing '%s' with:", name);
    for (size_t i = 0; i < num_values; i++) {
        DLOG(" %s", values[i]);
    }
    DLOG("\n");

    entry = get_conf(name, &index);
    if (entry == NULL) {
        Config.entries = sreallocarray(Config.entries,
                Config.num_entries + 1, sizeof(*Config.entries));
        memmove(&Config.entries[index + 1], &Config.entries[index],
                sizeof(*Config.entries) * (Config.num_entries - index));
        entry = &Config.entries[index];
        upper = smalloc(strlen(name) + 1);
        entry->name = upper;
        for (; *name != '\0'; name++, upper++) {
            upper[0] = toupper(name[0]);
        }
        upper[0] = '\0';
        entry->values = NULL;
        entry->num_values = 0;
        entry->long_value = 0;
        Config.num_entries++;
    }

    switch (mode) {
    case SET_CONF_MODE_SET:
        for (size_t i = 0; i < entry->num_values; i++) {
            free(entry->values[i]);
        }
        entry->values = sreallocarray(entry->values,
                num_values, sizeof(*entry->values));
        for (size_t i = 0; i < num_values; i++) {
            entry->values[i] = sstrdup(values[i]);
        }
        entry->num_values = num_values;
        break;

    case SET_CONF_MODE_APPEND:
        entry->values = sreallocarray(entry->values,
                num_values + entry->num_values, sizeof(*entry->values));
        for (size_t i = 0; i < num_values; i++) {
            if (get_entry_value_index(entry, values[i]) == SIZE_MAX) {
                entry->values[entry->num_values++] = strdup(values[i]);
            }
        }
        break;

    case SET_CONF_MODE_SUBTRACT:
        for (size_t i = 0; i < num_values; i++) {
            index = get_entry_value_index(entry, values[i]);
            if (index != SIZE_MAX) {
                entry->num_values--;
                free(entry->values[index]);
                memmove(&entry->values[index],
                        &entry->values[index + 1],
                        sizeof(*entry->values) * (entry->num_values - index));
            }
        }
        break;
    }

    if (entry->num_values != 0) {
        entry->long_value = strtol(entry->values[0], NULL, 0);
    }

    DLOG("'%s' is now:", entry->name);
    for (size_t i = 0; i < entry->num_values; i++) {
        DLOG(" %s", entry->values[i]);
    }
    DLOG("\n");

    env_len = entry->num_values == 0 ? 1 : 0;
    for (size_t i = 0; i < entry->num_values; i++) {
        env_len += strlen(entry->values[i]) + 1;
    }
    env = smalloc(env_len);
    env_i = 0;
    for (size_t i = 0, len; i < entry->num_values; i++) {
        if (i > 0) {
            env[env_i++] = ' ';
        }
        len = strlen(entry->values[i]);
        memcpy(&env[env_i], entry->values[i], len);
        env_i += len;
    }
    env[env_i] = '\0';
    setenv(entry->name, env, 1);
    free(env);
    return 0;
}

static inline void print_value(FILE *fp, char *val)
{
    if (val[0] == '\0') {
        fputc('\'', fp);
        fputc('\'', fp);
    }
    for (; val[0] != '\0'; val++) {
        switch (val[0]) {
        case ';':
        case '\'':
        case '\"':
        case ' ':
        case '\t':
            fputc('\\', fp);
            break;
        }
        fputc(val[0], fp);
    }
}

void dump_conf(FILE *fp)
{
    struct config_entry *entry;

    for (size_t i = 0; i < Config.num_entries; i++) {
        entry = &Config.entries[i];
        print_value(fp, entry->name);
        fputc(' ', fp);
        fputc('=', fp);
        for (size_t i = 0; i < entry->num_values; i++) {
            fputc(' ', fp);
            print_value(fp, entry->values[i]);
        }
        fputc('\n', fp);
    }
}

int check_conf(void)
{
    static const struct {
        const char *name;
        size_t num;
    } checks[] = {
        { "CC", 1 },
        { "BUILD", 1 },
        { "EXTENSIONS", EXT_TYPE_MAX },
        { "C_FLAGS", 0 },
        { "C_LIBS", 0 },
    };
    static const struct {
        const char *name;
        int type;
    } checks_ext[] = {
        { "EXT_BUILD", EXT_TYPE_OBJECT },
        { "EXT_SOURCE", EXT_TYPE_SOURCE },
        { "EXT_HEADER", EXT_TYPE_HEADER },
    };
    struct config_entry *entry, *exts_entry;

    for (size_t i = 0; i < ARRAY_SIZE(checks); i++) {
        entry = get_conf(checks[i].name, NULL);
        if (entry == NULL) {
            if (checks[i].num == 0) {
                set_conf(checks[i].name, NULL, 0, 0);
                continue;
            }
        } else if (checks[i].num == entry->num_values ||
                checks[i].num == 0) {
            continue;
        }
        fprintf(stderr, "can not parse because '%s'"
                "does not have exactly '%zu' values\n",
                checks[i].name, checks[i].num);
        return -1;
    }

    exts_entry = get_conf("extensions", NULL);
    for (size_t i = 0; i < ARRAY_SIZE(checks_ext); i++) {
        entry = get_conf(checks_ext[i].name, NULL);
        if (entry != NULL && entry->num_values == 1) {
            free(exts_entry->values[checks_ext[i].type]);
            exts_entry->values[checks_ext[i].type] = sstrdup(entry->values[0]);
        }
    }
    return 0;
}

bool find_autocar_conf(const char *name_or_path)
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

bool source_conf(const char *conf)
{
    set_conf("config_path", &conf, 1, SET_CONF_MODE_SET);
    return source_path(conf) == 0;
}

void set_default_conf(void)
{
    static const char *default_extensions[] = {
        [EXT_TYPE_OTHER] = "",
        [EXT_TYPE_SOURCE] = ".c",
        [EXT_TYPE_HEADER] = ".h",
        [EXT_TYPE_OBJECT] = ".o",
        [EXT_TYPE_EXECUTABLE] = "",
        [EXT_TYPE_FOLDER] = "",
    };
    static const char *default_compiler = "gcc";
    static const char *default_diff = "diff";
    static const char *default_build = "build";
    static const char *default_flags[] = {
        "-g", "-fsanitize=address", "-Wall", "-Wextra", "-Werror"
    };
    static const char *default_interval = "100";
    static const char *default_prompt = ">>> ";

    set_conf("cc", &default_compiler, 1, SET_CONF_MODE_SET);
    set_conf("diff", &default_diff, 1, SET_CONF_MODE_SET);
    set_conf("build", &default_build, 1, SET_CONF_MODE_SET);
    set_conf("c_flags", default_flags, ARRAY_SIZE(default_flags), SET_CONF_MODE_SET);
    set_conf("extensions", default_extensions, EXT_TYPE_MAX, SET_CONF_MODE_SET);
    set_conf("interval", &default_interval, 1, SET_CONF_MODE_SET);
    set_conf("prompt", &default_prompt, 1, SET_CONF_MODE_SET);
}

void clear_conf(void)
{
    struct config_entry *entry;

    for (size_t i = 0; i < Config.num_entries; i++) {
        entry = &Config.entries[i];
        free(entry->name);
        for (size_t i = 0; i < entry->num_values; i++) {
            free(entry->values[i]);
        }
        free(entry->values);
    }
}
