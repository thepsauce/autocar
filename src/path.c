#include "path.h"
#include "salloc.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

static int openat_dir(int at, const char *name, int *pfd, DIR **pdir)
{
    int fd;
    DIR *dir;

    fd = openat(at, name, O_DIRECTORY | O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    dir = fdopendir(fd);
    if (dir == NULL) {
        close(fd);
        return -1;
    }
    *pfd = fd;
    *pdir = dir;
    return 0;
}

/* finds a path that leads from `from` to `to` */
static char *relative_path(const char *from, const char *to)
{
    size_t from_len, to_len;
    size_t prefix;

    from_len = strlen(from);
    to_len = strlen(to);

    prefix = 0;
    while (from[prefix] == to[prefix]) {
        prefix++;
    }

    if (from_len == to_len && prefix == from_len) {
        /* they are exactly the same path */
        return strdup(".");
    }

    /* position right after a slash */
    while (prefix > 0 && from[prefix] != '/') {
        prefix--;
    }

    size_t c = 0;
    for (size_t i = prefix; i < from_len; i++) {
        if (from[i] == '/') {
            c++;
        }
    }

    char *relative = smalloc(3 * c + to_len - prefix + 1);
    size_t index = 0;
    for (size_t i = 0; i < c; i++) {
        relative[index++] = '.';
        relative[index++] = '.';
    }
    strcpy(&relative[index], &to[prefix]);
    return relative;
}

int init_path(struct path *path, const char *root)
{
    if (root[0] == '\0') {
        return -1;
    }

    memset(path, 0, sizeof(*path));

    char *const real = realpath(root, NULL);

    if (real == NULL) {
        return -1;
    }
    const size_t real_len = strlen(real);

    char cwd[real_len + 1];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        free(real);
        return -1;
    }

    char *rel = relative_path(cwd, real);
    size_t rel_len = strlen(rel);

    path->p = malloc(rel_len + 2);
    path->l = rel_len + 1;
    memcpy(path->p, rel, rel_len);
    path->p[rel_len] = '/';
    path->p[rel_len + 1] = '\0';
    path->ds = path->l;

    free(rel);
    if (openat_dir(0, real, &path->fd, &path->dir) == -1) {
        free(path->p);
        free(real);
        return -1;
    }
    free(real);
    return 0;
}

static int cat_path(struct path *path, const char *name, bool dir)
{
    const size_t len_name = strlen(name);
    path->p = srealloc(path->p, path->ds + len_name + 2);
    memcpy(&path->p[path->ds], name, len_name);
    path->l = path->ds + len_name;
    if (dir) {
        path->p[path->l++] = '/';
        path->ds = path->l;
    }
    path->p[path->l] = '\0';
    return 0;
}

static int move_down_path(struct path *path)
{
    if (path->n == 0) {
        return 1;
    }
    closedir(path->dir);
    path->n--;
    path->fd = path->st[path->n].fd;
    path->dir = path->st[path->n].dir;
    path->l = path->st[path->n].ps;
    path->ds = path->l;
    return 0;
}

int climb_up_path(struct path *path, const char *name)
{
    int fd;
    DIR *dir;
    size_t ds;

    if (openat_dir(path->fd, name, &fd, &dir) < 0) {
        return -1;
    }
    ds = path->ds;
    if (cat_path(path, name, true) < 0) {
        closedir(dir);
        return -1;
    }
    if (path->n + 1 > path->c) {
        path->c *= 2;
        path->c++;
        path->st = sreallocarray(path->st, path->c, sizeof(*path->st));
    }
    path->st[path->n].fd = path->fd;
    path->st[path->n].dir = path->dir;
    path->st[path->n].ps = ds;
    path->n++;
    path->fd = fd;
    path->dir = dir;
    return 0;
}

static struct dirent *read_dir_ignore_hidden(DIR *dir)
{
    struct dirent *ent;
    do {
        ent = readdir(dir);
        if (ent == NULL) {
            return NULL;
        }
    } while (ent->d_name[0] == '.');
    return ent;
}

struct dirent *next_deep_file(struct path *path)
{
    struct dirent *ent;

    ent = read_dir_ignore_hidden(path->dir);

next:
    while (ent == NULL) {
        if (move_down_path(path) == 1) {
            return NULL;
        }
        ent = read_dir_ignore_hidden(path->dir);
    }

    while (ent->d_type == DT_DIR) {
        if (climb_up_path(path, ent->d_name) < 0) {
            ent = read_dir_ignore_hidden(path->dir);
            goto next;
        }
        ent = read_dir_ignore_hidden(path->dir);
        if (ent == NULL) {
            goto next;
        }
    }
    if (cat_path(path, ent->d_name, false) < 0) {
        goto next;
    }
    return ent;
}

void clear_path(struct path *path)
{
    for (unsigned i = 0; i < path->n; i++) {
        closedir(path->st[i].dir);
    }
    free(path->p);
    free(path->st);
}
