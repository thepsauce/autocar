#ifndef WATCH_H
#define WATCH_H

#include <pthread.h>

struct file {
    char *path;
    int wd;
    char **files;
    size_t num_files;
};

extern struct file_list {
    struct file *p;
    size_t n;
    pthread_mutex_t lock;
} Files;

int init_watch(void);
int watch_file_or_directory(const char *path);

#endif

