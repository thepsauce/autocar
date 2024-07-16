#ifndef CONF_H
#define CONF_H

#include <stdbool.h>

#define EXT_TYPE_OTHER 0
#define EXT_TYPE_SOURCE 1
#define EXT_TYPE_HEADER 2
#define EXT_TYPE_OBJECT 3
#define EXT_TYPE_EXECUTABLE 4
#define EXT_TYPE_FOLDER 5
#define EXT_TYPE_MAX 6

extern struct config {
    /// path of the config file
    char *path;
    /// compiler
    char *cc;
    /// diff program
    char *diff;
    /// compiler flags
    char **c_flags;
    /// number of compiler flags
    size_t num_c_flags;
    /// linker libraries
    char **c_libs;
    /// number of linker libraries
    size_t num_c_libs;
    /// build folder
    char *build;
    /// file extensions of different file types
    char *exts[EXT_TYPE_MAX];
    /// rebuild interval in milliseconds
    long interval;
    /// output file for compiler errors
    char *err_file;
    /// prompt for the command line
    char *prompt;
} Config;

/**
 * @brief Looks for the autocar config file.
 *
 * Checks for a config file with given path relative to the current directory or
 * by checking for a name in the current directory and all parent directories.
 * It remains in the directory it found the config file in.
 *
 * @param name_or_path Name or path if it contains slashes.
 *
 * @return Whether a config file was found.
 */
bool find_autocar_config(const char *name_or_path);

/* additional error codes for `set_conf()` */

#define SET_CONF_SINGLE 1
#define SET_CONF_APPEND 2
#define SET_CONF_EXIST 3

/**
 * @brief Sets a variable in the config.
 *
 * @return One of the above error codes or -1 on failure, 0 on success.
 */
int set_conf(const char *name, char **args, size_t num_args, bool append);

/**
 * @brief Sources the found config file.
 *
 * This is called after `find_autocar_config()` with the same parameter (or any
 * other valid path). It opens a file at given path and parses it. Results are
 * stored in `Config`.
 *
 * @see find_autocar_config()
 * @see source_file()
 *
 * @param conf Path to the config file.
 *
 * @return Whether the config was in a valid format and the file was read
 * successfully.
 */
bool source_config(const char *conf);

/**
 * @brief Checks the config values for correctness.
 *
 * When `Config.sources`, `Config.tests` or `Config.build` do not exist, they
 * are created, if any `Config` parameter is `NULL`, the C default is assumed.
 *
 * @see source_config()
 *
 * @return Whether the config values are correct.
 */
bool check_config(void);

#endif

