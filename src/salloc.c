#include "salloc.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void *smalloc(size_t size)
{
    void *ptr;

    if (size == 0) {
        return NULL;
    }
    ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "malloc(%zu): %s\n",
                size, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *scalloc(size_t nmemb, size_t size)
{
    void *ptr;

    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    ptr = calloc(nmemb, size);
    if (ptr == NULL) {
        fprintf(stderr, "calloc(%zu, %zu): %s\n",
                nmemb, size, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *srealloc(void *ptr, size_t size)
{
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    ptr = realloc(ptr, size);
    if (ptr == NULL) {
        fprintf(stderr, "realloc(%p, %zu): %s\n",
                ptr, size, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *sreallocarray(void *ptr, size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0) {
        free(ptr);
        return NULL;
    }
    ptr = reallocarray(ptr, nmemb, size);
    if (ptr == NULL) {
        fprintf(stderr, "reallocarray(%p, %zu, %zu): %s\n",
                ptr, nmemb, size, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *sstrdup(const char *s)
{
    char *s_dup;

    s_dup = strdup(s);
    if (s_dup == NULL) {
        fprintf(stderr, "strdup(%s): %s\n",
                s, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return s_dup;
}

char *sasprintf(const char *fmt, ...)
{
    va_list l;
    char *s;

    va_start(l, fmt);
    if (vasprintf(&s, fmt, l) == -1) {
        fprintf(stderr, "vasprintf(%s): %s\n",
                fmt, strerror(errno));
        exit(EXIT_FAILURE);
    }
    va_end(l);
    return s;
}
