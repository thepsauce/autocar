#include "args.h"
#include "watch.h"
#include "path.h"
#include "salloc.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

pthread_t WatchThreadId;

struct file_list Files;

static char *alloc_sprintf(const char *fmt, ...)
{
    char *s;
    va_list l;
    int n;

    va_start(l, fmt);
    n = vsnprintf(NULL, 0, fmt, l);
    va_end(l);

    va_start(l, fmt);
    s = smalloc(n + 1);
    vsprintf(s, fmt, l);
    va_end(l);
    return s;
}

static struct file *search_file(const char *path, size_t *pIndex)
{
    size_t l, r;

    l = 0;
    r = Files.n;
    while (l < r) {
        const size_t m = (l + r) / 2;

        struct file *const file = Files.p[m];
        const int cmp = strcmp(file->path, path);
        if (cmp == 0) {
            if (pIndex != NULL) {
                *pIndex = m;
            }
            return file;
        }
        if (cmp < 0) {
            l = m + 1;
        } else {
            r = m;
        }
    }
    if (pIndex != NULL) {
        *pIndex = r;
    }
    return NULL;
}

static struct file *add_file(const char *path, size_t index)
{
    Files.p = sreallocarray(Files.p, Files.n + 1, sizeof(*Files.p));
    struct file *const file = smalloc(sizeof(*file));
    file->path = sstrdup(path);
    memmove(&Files.p[index + 1], &Files.p[index],
            sizeof(*Files.p) * (Files.n - index));
    Files.p[index] = file;
    Files.n++;
    return file;
}

static bool check_file_changed(const char *path)
{
    const size_t path_len = strlen(path);

    if (path_len < 3 || path[path_len - 2] != '.') {
        return false;
    }

    struct file *file;

    size_t index;
    file = search_file(path, &index);
    if (file == NULL) {
        file = add_file(path, index);
        if (file == NULL) {
            return false;
        }
    }

    if (file->has_changed) {
        return true;
    }

    if (file->path[path_len - 1] != 'c' &&
            file->path[path_len - 1] != 'h') {
        return false;
    }

    struct stat st;
    if (stat(file->path, &st) != 0) {
        return false;
    }
    if (st.st_mtime != file->st.st_mtime) {
        file->has_changed = true;
        file->st = st;
        return true;
    }
    file->st = st;

    if (file->path[path_len - 1] != 'c') {
        return false;
    }

    char *cmd = alloc_sprintf("gcc -MG -MM \"%s\"", file->path);
    FILE *const pp = popen(cmd, "r");

    free(cmd);

    if (pp == NULL) {
        return false;
    }

    char *str;
    size_t ct, lt;
    int c;

    ct = 128;
    str = smalloc(ct);
    lt = 0;

    while ((c = fgetc(pp)) != EOF) {
        if (lt + 1 > ct) {
            ct *= 2;
            str = srealloc(str, ct);
        }
        if (c == '\\') {
            c = fgetc(pp);
        } else if (c == ':') {
            break;
        }
        str[lt++] = c;
    }

    str[lt] = '\0';

    char *target;
    size_t dir_len;

    char *const name = strrchr(file->path, '/');
    if (name != NULL) {
        dir_len = name - file->path;
    } else {
        dir_len = 0;
    }
    target = smalloc(path_len + 1);
    for (size_t i = 0; i < dir_len; i++) {
        target[i] = file->path[i];
    }
    if (dir_len != 0) {
        target[dir_len++] = '/';
    }
    memcpy(&target[dir_len], str, lt);
    target[path_len] = '\0';

    while ((c = fgetc(pp)) != EOF) {
        if (lt + 2 > ct) {
            ct *= 2;
            str = srealloc(str, ct);
        }
        if (c == '\\') {
            c = fgetc(pp);
        } else if (c == ' ') {
            str[lt] = '\0';
            if (str[lt - 1] == 'h') {
                if (check_file_changed(str)) {
                    free(str);
                    pclose(pp);
                    return true;
                }
            }
            lt = 0;
            continue;
        }
        str[lt++] = c;
    }

    free(str);

    pclose(pp);
    return false;
}

static int compile_file(struct file *file)
{
    static const char *const default_flags =
        "-g -fsanitize=address -std=gnu99 -Wall -Wextra -Werror -Wpedantic";
    const char *const flags =
        Args.gcc_flags == NULL ? default_flags : Args.gcc_flags;
    char object[strlen(file->path) + 1];
    strcpy(object, file->path);
    object[sizeof(object) - 2] = '.';
    object[sizeof(object) - 1] = 'o';
    char *cmd = alloc_sprintf("gcc %s -c %s -o %s", flags, file->path, object);
    system(cmd);
    free(cmd);
    return 0;
}

static int check_sources(const char *name)
{
    struct path path;
    struct dirent *ent;

    if (init_path(&path, name) < 0) {
        return -1;
    }

    while ((ent = next_deep_file(&path)) != NULL) {
        if (ent->d_type == DT_REG) {
            if (check_file_changed(path.p)) {
                printf("has_changed: %s\n", path.p);
            }
        }
    }

    for (size_t i = 0; i < Files.n; i++) {
        struct file *const file = Files.p[i];
        if (file->has_changed && file->path[strlen(file->path) - 1] == 'c') {
            compile_file(file);
        }
        file->has_changed = false;
    }

    clear_path(&path);
    return 0;
}

void *watch_thread(void *unused)
{
    (void) unused;

    while (1) {
        pthread_mutex_lock(&Files.lock);
        if (Args.num_sources == 0) {
            check_sources("src");
        } else {
            for (size_t i = 0; i < Args.num_sources; i++) {
                struct stat st;
                if (stat(Args.sources[i], &st) != 0) {
                    fprintf(stderr, "stat(%s): %s\n",
                            Args.sources[i],
                            strerror(errno));
                    continue;
                }
                if (S_ISDIR(st.st_mode)) {
                    check_sources(Args.sources[i]);
                } else {
                    check_file_changed(Args.sources[i]);
                }
            }
        }
        pthread_mutex_unlock(&Files.lock);
        usleep(150000);
    }
}

int init_watch(void)
{
    const int err = pthread_create(&WatchThreadId, 0, watch_thread, NULL);
    if (err != 0) {
        fprintf(stderr, "error: could not create watch thread: %s\n",
                strerror(err));
        exit(EXIT_FAILURE);
    }
    return 0;
}
