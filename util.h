#ifndef UTIL_H_
#define UTIL_H_

#include <stddef.h>

void *umalloc(size_t size);

void *ucalloc(size_t memb, size_t nmemb);

void ufree(void *v);

#endif
