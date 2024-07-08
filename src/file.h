#ifndef FILE_H
#define FILE_H

#define FILE_SOURCE 0x01
#define FILE_TESTS 0x02
#define FILE_OBJECTS 0x04

#include <stdbool.h>
#include <sys/stat.h>

struct file {
    unsigned flags;
    char *path;
    struct stat st;
};

extern struct file_list {
    struct file *ptr;
    size_t num;
} Files;

bool collect_sources(const char *path);
bool collect_tests(const char *path);
bool compile_files(void);
bool run_tests(void);

#endif

