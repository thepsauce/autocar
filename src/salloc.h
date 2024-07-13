#ifndef SALLOC_H
#define SALLOC_H

#include <stdlib.h>

void *stalloc(size_t size);
void *strealloc(void *ptr, size_t size);
void *streallocarray(void *ptr, size_t nmemb, size_t size);
char *tsprintf(const char *fmt, ...);
void clear_temp(void);
void sgrow(void **pptr, size_t *psize, size_t nmemb, size_t new_size);
void *smalloc(size_t size);
void *scalloc(size_t nmemb, size_t size);
void *srealloc(void *ptr, size_t size);
void *sreallocarray(void *ptr, size_t nmemb, size_t size);
void *sstrdup(const char *s);

#endif

