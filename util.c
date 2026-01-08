#include <assert.h>
#include <stddef.h>
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
