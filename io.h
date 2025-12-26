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

	/* this is pointer to data based on op
     * data type is:
     * request on OP_RECV
     * response on OP_WRITE
     * and so on... maybe more..
     * */
	void *data;

	/* free list uitl contains POISON_PTR if used */
	struct io_data_s *_next;
} io_data_t;

typedef struct io_data_pool_s {
	size_t cap;
	io_data_t *objs; /* io_data object type */
	io_data_t *fl; /* free list */
} io_data_pool_t;

typedef struct io_s {
	io_data_pool_t pool; /* io data pool */
	struct io_uring ring; /* io uring ring */

	/* io uring recv/read buffers */
	struct io_uring_buf_ring *buffer_ring_meta;

	uint8_t *buffer_ring; /* io uring buffer data */
	size_t buffer_ring_size;
	uint16_t buffer_ring_tail;
	size_t buffer_len;
	uint16_t nrbuf;
} io_t;

int io_init(io_t *io, size_t pool_size, unsigned int conc, size_t nrbuf,
	    size_t buf_len);

void io_free(io_t *io);

int io_submit_accept(io_t *io, int fd);

int io_submit_accept_multishot(io_t *io, int fd);

int io_submit_recv_multishot(io_t *io, int fd);

int io_submit_send(io_t *io, int fd, uint8_t *buf, size_t len);

int io_submit_close(io_t *io, int fd);

int io_submit_cancel(io_t *io, int fd);

int io_submit(io_t *io);

int io_submit_and_wait(io_t *io);

io_data_t *io_get_data(struct io_uring_cqe *cqe);

void io_free_data(io_t *io, struct io_uring_cqe *cqe);

uint8_t *io_get_recv_buffer(io_t *io, unsigned short bid);

ssize_t io_get_recv_buffer_len(struct io_uring_cqe *cqe);

unsigned short io_get_recv_buffer_id(struct io_uring_cqe *cqe);

void io_recycle_recv_buffer(io_t *io, unsigned short bid);

#endif
