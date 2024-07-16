#ifndef FILE_H
#define FILE_H

/// if the file exists on the file system
#define FLAG_EXISTS 0x1
/// if the object has a function called `main()`
#define FLAG_HAS_MAIN 0x2
/// if the file is a test
#define FLAG_IS_TEST 0x4
/// toggled after `add_file()`
#define FLAG_IS_FRESH 0x8

#include <stdbool.h>
#include <sys/stat.h>

/**
 * A file is a reference to a path, its main purpose is to cache data so it does
 * not need to be recomputed every time.
 */
struct file {
    /// full relative path of this file
    char *path;
    /// points at the file extension in `path`
    char *ext;
    /// extension type of this file ('EXT_TYPE_*')
    int type;
    /// flags of this file (`FLAG_*`)
    int flags;
    /// stat information about this file
    struct stat st;
    struct file **related;
};

/**
 * The file list has all files and is sorted by `path`.
 */
extern struct file_list {
    /// base pointer to the file list
    struct file **ptr;
    /// number of elements in the list
    size_t num;
} Files;

/**
 * The arrow '->' means: "This influences that".
 * .c -> .o
 * .h -> .h
 * .h -> .c
 * .o -> exec
 *
 * If the left changes, the right should also change.
 */
struct pair {
    struct file *left;
    struct file *right;
};

/**
 * Sorted by the pointer value of `left`.
 */
extern struct pair_list {
    struct pair *ptr;
    size_t num;
} Relation;

void add_pair(struct file *left, struct file *right);
void get_pairs(struct file *file, struct pair **ppairs, size_t *pnum);

/**
 * @brief Makes a file object and adds it to the file list.
 *
 * Checks if a file with given parameters already exists and returns this file,
 * it also uses `stat_file()` on it. If it does not exist, it makes a file using
 * given `folder`, `name` and `type`, constructs the `path` and then adds this
 * to the file list.
 *
 * @param path Path of the file.
 * @param type Type of the file (`EXT_TYPE_*`).
 * @param flags Flags of the file or'd together (`FLAG_*`).
 *
 * @return The allocated file.
 */
struct file *add_file(char *path, int type, int flags);

/**
 * @brief Find files in directories specified in the config.
 *
 * Collects all files it finds in either Config.sources or Config.tests
 * and adds them to the file list. If a file already exists on the file list,
 * then nothing happens.
 *
 * @return Whether all directories were accessible.
 */
bool collect_files(void);

/**
 * Build all files.
 */
bool build_objects(void);

/**
 * Link all executables.
 */
bool link_executables(void);

/**
 * Run all tests.
 */
bool run_tests(void);

#endif
