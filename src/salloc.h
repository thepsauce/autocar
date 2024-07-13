#ifndef SALLOC_H
#define SALLOC_H

#include <stdlib.h>

/**
 * @brief Like `malloc()` but exit when the allocation fails.
 */
void *smalloc(size_t size);

/**
 * @brief Like `malloc()` but exit when the allocation fails.
 */
void *scalloc(size_t nmemb, size_t size);

/**
 * @brief Like `realloc()` but exit when the allocation fails.
 */
void *srealloc(void *ptr, size_t size);

/**
 * @brief Like `reallocarray()` but exit when the allocation fails.
 */
void *sreallocarray(void *ptr, size_t nmemb, size_t size);

/**
 * @brief Like `strdup()` but exit when the allocation fails.
 */
void *sstrdup(const char *s);

#endif

