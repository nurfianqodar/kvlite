#ifndef REQUEST_H
#define REQUEST_H

#ifndef REQUEST_BUFFER_LEN
#define REQUEST_BUFFER_LEN 4096
#endif

#include <stdint.h>
typedef enum request_state_e {
	RS_NEED_LEN,
	RS_NEED_BODY,
	RS_DONE,
} request_state_t;

typedef struct request_s {
	request_state_t state;
	uint32_t message_len;
	uint8_t buffer[REQUEST_BUFFER_LEN];
	struct request_s *_next; // pool utility for freelist
} request_t;

#endif
