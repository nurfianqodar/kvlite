#ifndef SERVER_H_
#define SERVER_H_

#include <liburing.h>
#include <stdint.h>

typedef struct server_s {
	int fd;
	struct io_uring ring;
} server_t;

int server_init(server_t *s, const char *host, uint16_t port);

int server_start(server_t *s);

void server_deinit(server_t *s);

#endif
