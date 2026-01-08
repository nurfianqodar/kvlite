#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

void *umalloc(size_t size)
{
	void *data = malloc(size);
	assert(data != NULL);
	return data;
}

void *ucalloc(size_t memb, size_t nmemb)
{
	void *data = calloc(memb, nmemb);
	assert(data != NULL);
	return data;
}

void ufree(void *v)
{
	if (v == NULL) {
		return;
	}
	free(v);
}

uint32_t read_u32_be(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}
