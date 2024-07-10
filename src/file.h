#ifndef FILE_H
#define FILE_H

#define FILE_SOURCE 0x01
#define FILE_HEADER 0x02
#define FILE_OBJECT 0x04
#define FILE_OUTPUT 0x8
#define FILE_INPUT 0x10
#define FILE_DATA 0x20
#define FILE_TYPE_MASK 0xff

#define FILE_TYPE(f) ((f) & FILE_TYPE_MASK)

#define FILE_EXISTS 0x100
#define FILE_HAS_MAIN 0x200
#define FILE_HAS_UPDATE 0x400
#define FILE_IS_TEST 0x0800

#define FILE_FLAGS(f) ((f) & ~FILE_TYPE_MASK)

#include <stdbool.h>
#include <sys/stat.h>

struct file {
    unsigned flags;
    char *name;
    char *path;
    struct stat st;
};

extern struct file_list {
    struct file *ptr;
    size_t num;
} Files;

bool compile_files(void);
bool link_binaries(void);
bool run_tests(void);

#endif

