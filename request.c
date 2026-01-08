#include "request.h"
#include "util.h"
#include <stdio.h>

int request_parse(request_t *req, uint8_t *buf, size_t len)
{
	if (len < 8) {
		return REQ_ERR_TOO_SHORT;
	}
	req->kind = read_u32_be(buf);
	req->body_len = read_u32_be(buf + 4);
	if (req->body_len > MAX_REQUEST_BODY) {
		return REQ_ERR_TOO_LONG;
	}
	if ((len - 8) < req->body_len) {
		return REQ_ERR_TOO_SHORT;
	}
	req->body = buf + 8;
	return 0;
}

void request_debug(request_t *req)
{
	printf("kind = %d\n", req->kind);
	printf("len  = %d\n", req->body_len);
	printf("body = %.*s\n", req->body_len, req->body);
}
