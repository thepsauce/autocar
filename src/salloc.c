#include "salloc.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct {
    void **ptrs;
    size_t num;
} Stalloc;

static void *register_pointer(void *ptr)
{
    Stalloc.ptrs = sreallocarray(Stalloc.ptrs, Stalloc.num,
            sizeof(*Stalloc.ptrs));
    Stalloc.ptrs[Stalloc.num++] = ptr;
    return ptr;
}

void *stalloc(size_t size)
{
    return register_pointer(smalloc(size));
}

static void *change_pointer(void *ptr, void *new_ptr)
{
    for (size_t i = 0; i < Stalloc.num; i++) {
        if (Stalloc.ptrs[i] == ptr) {
            Stalloc.ptrs[i] = new_ptr;
            return ptr;
        }
    }
    fprintf(stderr, "change_pointer: pointer is not temporary\n");
    exit(EXIT_FAILURE);
}

void *strealloc(void *ptr, size_t size)
{
    void *new_ptr;

    if (ptr == NULL) {
        return stalloc(size);
    }
    new_ptr = srealloc(ptr, size);
    change_pointer(ptr, new_ptr);
    return ptr;
}

void *streallocarray(void *ptr, size_t nmemb, size_t size)
{
    void *new_ptr;

    if (ptr == NULL) {
        return register_pointer(reallocarray(NULL, nmemb, size));
    }
    new_ptr = sreallocarray(ptr, nmemb, size);
    change_pointer(ptr, new_ptr);
    return ptr;
}

void make_perm(void *ptr)
{
    change_pointer(ptr, NULL);
}

void clear_temp(void)
{
    for (size_t i = 0; i < Stalloc.num; i++) {
        free(Stalloc.ptrs[i]);
    }
    free(Stalloc.ptrs);
}

char *tsprintf(const char *fmt, ...)
{
    va_list l;
    int n;
    char *s;

    va_start(l, fmt);
    n = vsnprintf(NULL, 0, fmt, l);
    va_end(l);
    if (n < 0) {
        fprintf(stderr, "vsnprintf: %s\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    s = stalloc(n + 1);
    va_start(l, fmt);
    vsprintf(s, fmt, l);
    va_end(l);
    return s;
}

void sgrow(void **pptr, size_t *psize, size_t nmemb, size_t new_size)
{
    if (*psize >= new_size) {
        return;
    }
    *pptr = sreallocarray(*pptr, nmemb, new_size);
    *psize = new_size;
}

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
