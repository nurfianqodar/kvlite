#ifndef REQUEST_H_
#define REQUEST_H_

#include <stddef.h>
#include <stdint.h>

typedef uint32_t request_kind_t;

#define REQUEST_KIND_PING (request_kind_t)(0)
#define REQUEST_KIND_CONN (request_kind_t)(1) // connect
#define REQUEST_KIND_PUT (request_kind_t)(1 < 1) // put new data
#define REQUEST_KIND_GET (request_kind_t)(1 < 2) // get data by key
#define REQUEST_KIND_MOD (request_kind_t)(1 < 3) // modify data by key
#define REQUEST_KIND_DEL (request_kind_t)(1 < 4) // delete data by key
#define REQUEST_KIND_UNDEF (request_kind_t)(1 < 30) // undefined message kind
#define REQUEST_KIND_OFF (request_kind_t)(1 < 31) // disconnect

#define REQ_ERR_TOO_SHORT -1
#define REQ_ERR_INVALID_KIND -2
#define REQ_ERR_TOO_LONG -3

#define MAX_REQUEST_BODY (size_t)(8192 - 8)

typedef struct request_s {
	request_kind_t kind;
	uint32_t body_len;
	uint8_t *body;
} request_t;

int request_parse(request_t *req, uint8_t *buf, size_t len);

void request_debug(request_t *req);

#endif
