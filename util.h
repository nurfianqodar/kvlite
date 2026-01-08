#ifndef UTIL_H_
#define UTIL_H_

#include <stddef.h>
#include <stdint.h>

void *umalloc(size_t size);

void *ucalloc(size_t memb, size_t nmemb);

void ufree(void *v);

uint32_t read_u32_be(const uint8_t *p);

#endif
