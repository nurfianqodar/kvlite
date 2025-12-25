#ifndef IO_H
#define IO_H

#include <liburing.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef IO_NRBUFS
#define IO_NRBUFS 1024
#endif

#ifndef IO_BUF_LEN
#define IO_BUF_LEN 4096
#endif

#define IO_BGID_READ 1

typedef enum io_op_e {
	OP_UNSPECIFIED,
	OP_ACCEPT,
	OP_RECV,
	OP_SEND,
	OP_CANCEL,
	OP_CLOSE,
} io_op_t;

typedef struct io_data_s {
	io_op_t op; /* operation id */
	int fd; /* file descriptor */
	struct sockaddr_storage addr; /* fd address */
	socklen_t addr_len; /* address size */
	void *data; /* pointer to write buffer. ONLY USE ON WRITE */
	struct io_data_s *_next; /* free list uitl
                                contains POISON_PTR if used */
} io_data_t;

typedef struct io_data_pool_s {
	size_t cap;
	io_data_t *objs; /* io_data object type */
	io_data_t *fl; /* free list */
} io_data_pool_t;

typedef struct io_s {
	io_data_pool_t pool; /* io data pool */
	struct io_uring ring; /* io uring ring */
	struct io_uring_buf_ring *br; /* io uring buffers */
	uint8_t *buffer_pool;
} io_t;

int io_init(io_t *io, size_t pool_size, unsigned int conc);

void io_free(io_t *io);

int io_submit_accept(io_t *io, int fd);

int io_submit_accept_multishot(io_t *io, int fd);

int io_submit_recv_multishot(io_t *io, int fd);

int io_submit_send(io_t *io, int fd, uint8_t *buf, size_t len);

int io_submit_close(io_t *io, int fd);

int io_submit_cancel(io_t *io, int fd);

io_data_t *io_get_data(struct io_uring_cqe *cqe);

void io_free_data(struct io_uring_cqe *cqe);

uint8_t *io_get_recv_result(io_t *io, struct io_uring_cqe *cqe);

ssize_t io_get_recv_result_len(struct io_uring_cqe *cqe);

#endif
