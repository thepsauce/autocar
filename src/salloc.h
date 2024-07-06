#ifndef SALLOC_H
#define SALLOC_H

#include <stdlib.h>

void *smalloc(size_t size);
void *scalloc(size_t nmemb, size_t size);
void *srealloc(void *ptr, size_t size);
void *sreallocarray(void *ptr, size_t nmemb, size_t size);
void *sstrdup(const char *s);

#endif

