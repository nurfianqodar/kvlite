#ifndef SERVER_H
#define SERVER_H

#include "io.h"
#include <stddef.h>
#include <stdint.h>

typedef struct server_s {
	io_t io;
	unsigned char host[47];
	uint16_t port;
	int fd;
} server_t;

int server_init(server_t *server, const char *host, size_t host_len,
		uint16_t port);

int server_start(server_t *server);

#endif
