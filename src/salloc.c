#include "salloc.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>

void *smalloc(size_t size)
{
    void *const p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "malloc(%zu): %s\n",
                size, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}

void *scalloc(size_t nmemb, size_t size)
{
    void *const p = calloc(nmemb, size);
    if (p == NULL) {
        fprintf(stderr, "calloc(%zu, %zu): %s\n",
                nmemb, size, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}

void *srealloc(void *ptr, size_t size)
{
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
    char *const s_dup = strdup(s);
    if (s_dup == NULL) {
        fprintf(stderr, "strdup(%s): %s\n",
                s, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return s_dup;
}
