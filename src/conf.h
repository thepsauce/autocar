#ifndef CONF_H
#define CONF_H

#include <stdbool.h>

extern struct config {
    char *cc;
    char **c_flags;
    size_t num_c_flags;
    char **c_libs;
    size_t num_c_libs;
    char *sources;
    char *tests;
    char *build;
    long interval;
} Config;

bool find_autocar_config(const char *name_or_path);
bool source_config(const char *conf);
bool check_config(void);

#endif

