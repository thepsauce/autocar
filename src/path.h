#ifndef PATH_H
#define PATH_H

#include <dirent.h>

struct path {
    struct path_stack {
        /* file descriptor of below directory */
        int fd;
        DIR *dir;
        size_t ps;
    } *st;
    /* size and capacity of the stack */
    unsigned n, c;
    /* current path */
    char *p;
    /* length of the current path */
    size_t l;
    /* index of the directory */
    size_t ds;
    /* current directory */
    int fd;
    DIR *dir;
};

int init_path(struct path *path, const char *root);
int climb_up_path(struct path *path, const char *name);
struct dirent *next_deep_file(struct path *path);
void clear_path(struct path *path);

#endif

