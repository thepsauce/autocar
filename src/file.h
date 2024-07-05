#ifndef FILE_H
#define FILE_H

struct file {
    char name[MAX_NAME];
    struct file *child;
    struct file *next;
    int wd;
};

struct file *Root;
struct file *Current;
struct file *Home;

void add_path(const char *path);

#endif

