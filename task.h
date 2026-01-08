#ifndef TASK_H_
#define TASK_H_

#include <liburing.h>
#include <stddef.h>
#include <stdint.h>

typedef enum op_e {
	OP_ACCEPT,
	OP_RECV,
	OP_SEND,
	OP_CLOSE,
} op_t;

typedef struct send_data_s {
	uint8_t *buf;
	size_t buf_len;
	size_t sended_len;
} send_data_t;

typedef struct recv_data_s {
	uint8_t *buf;
	size_t buf_len;
	size_t recvd_len;
} recv_data_t;

typedef struct task_s {
	op_t op;
	int fd;
	void *data;
} task_t;

task_t *task_new_accept(int fd);

task_t *task_new_recv(int fd, size_t buf_len);
recv_data_t *task_recv_get_data(task_t *task);

task_t *task_new_send(int fd, uint8_t *msg, size_t len);
send_data_t *task_send_get_data(task_t *task);

task_t *task_new_close(int fd);

void task_free(task_t *task);

void create_submission(struct io_uring *ring, task_t *task);

#endif
