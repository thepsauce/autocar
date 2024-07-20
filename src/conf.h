#ifndef CONF_H
#define CONF_H

#include <stdbool.h>
#include <stdio.h>

#define EXT_TYPE_OTHER 0
#define EXT_TYPE_SOURCE 1
#define EXT_TYPE_HEADER 2
#define EXT_TYPE_OBJECT 3
#define EXT_TYPE_EXECUTABLE 4
#define EXT_TYPE_FOLDER 5
#define EXT_TYPE_MAX 6

struct config_entry {
    int type;
    char *name;
    char **values;
    size_t num_values;
};

extern struct config {
    struct config_entry *entries;
    size_t num_entries;
} Config;

/**
 * @brief Gets the entry with given name.
 *
 * @param name Name of the entry to search for.
 * @param pindex Where to store the optimal index, may be `NULL`.
 *
 * @return Found entry or `NULL` if none was found.
 */
struct config_entry *get_conf(const char *name, size_t *pindex);

/**
 * @brief Gets the entry with given name with given length.
 *
 * @param name      Name of the entry to search for.
 * @param name_len  Length of the entry to search for.
 * @param pindex    Where to store the optimal index, may be `NULL`.
 *
 * @return Found entry or `NULL` if none was found.
 */
struct config_entry *get_conf_l(const char *name, size_t name_len, size_t *pindex);

#define SET_CONF_MODE_SET 0
#define SET_CONF_MODE_APPEND 1
#define SET_CONF_MODE_SUBTRACT 2

/**
 * @brief Sets entry with given name to given values, if no entry exists with
 * that name, it is created.
 *
 * @param name Name of the entry to set.
 * @param values New values of the entry.
 * @param num_values Number of the new values.
 * @param Mode to use (see above).
 *
 * @return Always 0.
 */
int set_conf(const char *name, const char **values,
        size_t num_values, int mode);

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
bool find_autocar_conf(const char *name_or_path);

/**
 * @brief Prints all variable correctly escaped to given file pointer
 */
void dump_conf(FILE *fp);

/**
 * @brief Checks whether the config is ready for compiling.
 */
int check_conf(void);

/**
 * @brief Sources the found config file.
 *
 * This is called after `find_autocar_config()` with the same parameter (or any
 * other valid path). It opens a file at given path and parses it. Results are
 * stored in `Config`.
 *
 * @see find_autocar_conf()
 * @see source_file()
 *
 * @param conf Path to the config file.
 *
 * @return Whether the config was in a valid format and the file was read
 * successfully.
 */
bool source_conf(const char *conf);

/**
 * @brief Sets the default config values.
 *
 * @see source_conf()
 */
void set_default_conf(void);

/**
 * @brief Clears all variables in the config.
 */
void clear_conf(void);

#endif

