#define _GNU_SOURCE

#include "io.h"
#include "util.h"
#include <liburing.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// =================================================
// private API's
// =================================================

static int _io_data_pool_init(io_data_pool_t *io_data_pool, size_t cap)
{
	if (io_data_pool == NULL || cap == 0)
		return -1;

	io_data_pool->cap = cap;
	io_data_pool->fl = NULL;
	io_data_pool->objs = calloc(cap, sizeof(io_data_t));
	if (io_data_pool->objs == NULL)
		return -1;

	for (size_t i = cap; i > 0; --i) {
		size_t idx = i - 1;
		io_data_t *io_data = &io_data_pool->objs[idx];
		io_data->fd = -1;
		io_data->op = OP_UNSPECIFIED;
		io_data->data = NULL;
		io_data->addr_len = 0;
		memset(&io_data->addr, 0, sizeof(io_data->addr));

		io_data->_next = io_data_pool->fl;
		io_data_pool->fl = io_data;
	}

	return 0;
}

static void _io_data_pool_free(io_data_pool_t *io_data_pool)
{
	if (io_data_pool == NULL)
		return;

	for (size_t i = io_data_pool->cap; i > 0; --i) {
		size_t idx = i - 1;
		io_data_t *io_data = &io_data_pool->objs[idx];

		// close fd if any valid fd
		if (io_data->fd >= 0) {
			close(io_data->fd);
		}
		io_data->fd = -1;
		io_data->op = OP_UNSPECIFIED;
		io_data->addr_len = 0;
		io_data->data = NULL;
		memset(&io_data->addr, 0, sizeof(io_data->addr));
	}

	io_data_pool->fl = NULL;
	io_data_pool->cap = 0;
	free(io_data_pool->objs);
}

static io_data_t *_io_data_alloc(io_data_pool_t *io_data_pool)
{
	if (io_data_pool == NULL)
		return NULL;
	if (io_data_pool->fl == NULL)
		return NULL;

	io_data_t *io_data;
	io_data = io_data_pool->fl;
	io_data_pool->fl = io_data_pool->fl->_next;
	io_data->_next = POISON_PTR;

	return io_data;
}

void _io_data_free(io_data_pool_t *io_data_pool, io_data_t *io_data)
{
	if (io_data == NULL || io_data_pool == NULL)
		return;

	io_data->fd = -1;
	io_data->op = OP_UNSPECIFIED;
	io_data->addr_len = 0;
	io_data->data = NULL;
	memset(&io_data->addr, 0, sizeof(io_data->addr));

	io_data->_next = io_data_pool->fl;
	io_data_pool->fl = io_data;
}

static int _io_data_prep(io_data_t *io_data, io_op_t op, int fd)
{
	if (op == OP_UNSPECIFIED)
		return -1;

	if (op == OP_ACCEPT) {
		io_data->addr_len = sizeof io_data->addr;
	}
	io_data->op = op;
	io_data->fd = fd;

	return 0;
}

static int _io_create_buffer_ring(io_t *io, size_t nrbuf, size_t buf_len,
				  int buf_gid)
{
	const size_t ALIGNMENT = 4096;
	const size_t BUFFER_RING_METADATA_SIZE =
		nrbuf * sizeof(struct io_uring_buf);
	const size_t BUFFER_RING_SIZE = nrbuf * buf_len;

	int ret;
	struct io_uring_buf_reg reg;

	// Allocate buffer ring metadata
	// assign io->buffer_ring_meta
	ret = posix_memalign((void *)&io->buffer_ring_meta, ALIGNMENT,
			     BUFFER_RING_METADATA_SIZE);
	if (ret != 0)
		goto ret_err;

	reg = (struct io_uring_buf_reg){
		.bgid = buf_gid,
		.ring_addr = (unsigned long)io->buffer_ring_meta,
		.ring_entries = nrbuf,
	};

	// register to ring
	ret = io_uring_register_buf_ring(&io->ring, &reg, 0);
	if (ret < 0)
		goto clear_meta;

	// Allocate buffer ring
	// assign io->buffer_ring
	ret = posix_memalign((void *)&io->buffer_ring, ALIGNMENT,
			     BUFFER_RING_SIZE);
	if (ret != 0)
		goto unreg_meta;

	io->buffer_ring_tail = 0;
	io->buffer_ring_size = BUFFER_RING_SIZE;

	// populate buffer ring
	int mask = io_uring_buf_ring_mask(nrbuf);
	for (int i = 0; i < nrbuf; i++) {
		uint8_t *cur_addr = (uint8_t *)io->buffer_ring +
				    (i * buf_len); /* current buffer address
                                      add */
		unsigned short buffer_id = i;
		int offset = i;
		io_uring_buf_ring_add(io->buffer_ring_meta, cur_addr, buf_len,
				      buffer_id, mask, offset);
		io->buffer_ring_tail++;
	}
	// publish to kernel
	io_uring_buf_ring_advance(io->buffer_ring_meta, nrbuf);
	return 0;

unreg_meta:
	io_uring_unregister_buf_ring(&io->ring, buf_gid);
clear_meta:
	free(io->buffer_ring_meta);
ret_err:
	return -1;
}

// =================================================
// public API's
// =================================================

/* io utilities initialization
 * signature:
 * `int io_init(io_t *io, size_t pool_size, unsigned int conc, size_t nrbuf, size_t buf_len);`
 */
int io_init(io_t *io, size_t pool_size, unsigned int conc, size_t nrbuf,
	    size_t buf_len)
{
	int ret;
	unsigned short buffer_gid = 1;

	ret = io_uring_queue_init(conc, &io->ring, 0);
	if (ret != 0)
		goto ret_err;

	ret = _io_data_pool_init(&io->pool, pool_size);
	if (ret < 0)
		goto ring_clear;

	// buffer ring initialization
	ret = _io_create_buffer_ring(io, nrbuf, buf_len, buffer_gid);
	if (ret < 0)
		goto pool_clear;

	return 0;

pool_clear:
	_io_data_pool_free(&io->pool);
ring_clear:
	io_uring_queue_exit(&io->ring);
ret_err:
	return -1;
}

void io_free(io_t *io)
{
	if (io == NULL)
		return;

	struct io_uring_cqe *cqe;
	while (io_uring_peek_cqe(&io->ring, &cqe) == 0) {
		io_uring_cqe_seen(&io->ring, cqe);
	}

	io_uring_unregister_buf_ring(&io->ring, IO_BGID_READ);
	io_uring_queue_exit(&io->ring);
	if (io->buffer_ring) {
		free(io->buffer_ring);
		io->buffer_ring = NULL;
	}

	if (io->buffer_ring_meta) {
		free(io->buffer_ring_meta);
		io->buffer_ring_meta = NULL;
	}
	_io_data_pool_free(&io->pool);
}

int io_submit_accept(io_t *io, int fd)
{
	io_data_t *io_data;
	struct io_uring_sqe *sqe;

	io_data = _io_data_alloc(&io->pool);
	if (io_data == NULL)
		goto ret_err;
	_io_data_prep(io_data, OP_ACCEPT, fd);

	sqe = io_uring_get_sqe(&io->ring);
	if (sqe == NULL)
		goto iod_free;
	io_uring_prep_accept(sqe, fd, (struct sockaddr *)&io_data->addr,
			     &io_data->addr_len, 0);
	io_uring_sqe_set_data(sqe, io_data);
	return 0;

iod_free:
	_io_data_free(&io->pool, io_data);
ret_err:
	return -1;
}

int io_submit_accept_multishot(io_t *io, int fd)
{
	io_data_t *io_data;
	struct io_uring_sqe *sqe;

	io_data = _io_data_alloc(&io->pool);
	if (io_data == NULL)
		goto ret_err;
	_io_data_prep(io_data, OP_ACCEPT, fd);

	sqe = io_uring_get_sqe(&io->ring);
	if (sqe == NULL)
		goto iod_free;

	io_uring_prep_multishot_accept(sqe, fd, NULL, NULL, 0);
	io_uring_sqe_set_data(sqe, io_data);
	return 0;

iod_free:
	_io_data_free(&io->pool, io_data);
ret_err:
	return -1;
}

int io_submit_recv_multishot(io_t *io, int fd)
{
	io_data_t *io_data;
	struct io_uring_sqe *sqe;

	io_data = _io_data_alloc(&io->pool);
	if (io_data == NULL)
		goto ret_err;
	_io_data_prep(io_data, OP_RECV, fd);

	sqe = io_uring_get_sqe(&io->ring);
	if (sqe == NULL)
		goto iod_free;
	io_uring_prep_recv_multishot(sqe, fd, NULL, 0, 0);
	sqe->buf_group = IO_BGID_READ;
	sqe->flags |= IOSQE_BUFFER_SELECT;
	io_uring_sqe_set_data(sqe, io_data);
	return 0;

iod_free:
	_io_data_free(&io->pool, io_data);
ret_err:
	return -1;
}

int io_submit_send(io_t *io, int fd, uint8_t *buf, size_t len)
{
	io_data_t *io_data;
	struct io_uring_sqe *sqe;

	io_data = _io_data_alloc(&io->pool);
	if (io_data == NULL)
		goto ret_err;
	_io_data_prep(io_data, OP_SEND, fd);
	io_data->data = buf; /* if buffer is allocated on heap or any 
                            allocator this field help to free buffer */

	sqe = io_uring_get_sqe(&io->ring);
	if (sqe == NULL)
		goto iod_free;
	io_uring_prep_send(sqe, fd, buf, len, 0);
	io_uring_sqe_set_data(sqe, io_data);

	return 0;

iod_free:
	_io_data_free(&io->pool, io_data);
ret_err:
	return -1;
}

int io_submit_close(io_t *io, int fd)
{
	io_data_t *io_data;
	struct io_uring_sqe *sqe;

	io_data = _io_data_alloc(&io->pool);
	if (io_data == NULL)
		goto ret_err;
	_io_data_prep(io_data, OP_CLOSE, fd);

	sqe = io_uring_get_sqe(&io->ring);
	if (sqe == NULL)
		goto iod_free;
	io_uring_prep_close(sqe, fd);
	io_uring_sqe_set_data(sqe, io_data);

	return 0;

iod_free:
	_io_data_free(&io->pool, io_data);
ret_err:
	return -1;
}

int io_submit_cancel(io_t *io, int fd)
{
	io_data_t *io_data;
	struct io_uring_sqe *sqe;

	io_data = _io_data_alloc(&io->pool);
	if (io_data == NULL)
		goto ret_err;
	_io_data_prep(io_data, OP_CANCEL, fd);

	sqe = io_uring_get_sqe(&io->ring);
	if (sqe == NULL)
		goto iod_free;
	io_uring_prep_close(sqe, fd);
	io_uring_sqe_set_data(sqe, io_data);

	return 0;

iod_free:
	_io_data_free(&io->pool, io_data);
ret_err:
	return -1;
}

int io_submit(io_t *io)
{
	int submited;
	submited = io_uring_submit(&io->ring);
	return submited;
}

int io_submit_and_wait(io_t *io)
{
	return io_uring_submit_and_wait(&io->ring, 1);
}

io_data_t *io_get_data(struct io_uring_cqe *cqe)
{
	if (cqe == NULL)
		return NULL;

	return (io_data_t *)cqe->user_data;
}

void io_free_data(io_t *io, struct io_uring_cqe *cqe)
{
	if (cqe == NULL)
		return;

	io_data_t *io_data = io_get_data(cqe);
	_io_data_free(&io->pool, io_data);
}

uint8_t *io_get_recv_buffer(io_t *io, unsigned short bid)
{
	uint8_t *data_ptr = io->buffer_ring + (bid * IO_BUF_LEN);
	return data_ptr;
}

ssize_t io_get_recv_buffer_len(struct io_uring_cqe *cqe)
{
	io_data_t *io_data = io_get_data(cqe);
	if (io_data == NULL)
		return -1;
	if (io_data->op != OP_RECV)
		return -1;
	return cqe->res;
}

unsigned short io_get_recv_buffer_id(struct io_uring_cqe *cqe)
{
	return cqe->flags >> IORING_CQE_BUFFER_SHIFT;
}

void io_recycle_recv_buffer(io_t *io, unsigned short bid)
{
	uint8_t *addr = (uint8_t *)io->buffer_ring + (bid * IO_BUF_LEN);
	io_uring_buf_ring_add(io->buffer_ring_meta, addr, io->buffer_len, bid,
			      io_uring_buf_ring_mask(io->nrbuf),
			      io->buffer_ring_tail);

	io->buffer_ring_tail++;
	io_uring_buf_ring_advance(io->buffer_ring_meta, 1);
}
