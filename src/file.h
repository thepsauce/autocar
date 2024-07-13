#ifndef FILE_H
#define FILE_H

/// if the file exists on the file system
#define FLAG_EXISTS 0x1
/// if the object has a function called `main()`
#define FLAG_HAS_MAIN 0x2

#include <stdbool.h>
#include <sys/stat.h>

struct file {
    /// the folder this file is contained in (`FOLDER_*`)
    int folder;
    /// flags of this file (`FLAG_*`)
    int flags;
    /// extension type of this file ('EXT_TYPE_*')
    int type;
    /// name of this file, this can contain slashes
    char *name;
    /// full relative or absolute path of this file
    char *path;
    /// stat information about this file
    struct stat st;
    /// last time of input file (for test executables)
    time_t last_input;
    /// last time of data file (for test executables)
    time_t last_data;
};

extern struct file_list {
    /// base pointer to the file list
    struct file **ptr;
    /// number of elements in the list
    size_t num;
} Files;

/**
 *
 */
struct file *add_path(const char *path);

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

