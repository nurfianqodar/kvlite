#include "task.h"
#include "util.h"
#include <assert.h>
#include <string.h>
#include <liburing.h>

task_t *task_new_accept(int fd)
{
	task_t *task = umalloc(sizeof(task_t));
	task->fd = fd;
	task->op = OP_ACCEPT;
	task->data = NULL;
	return task;
}

task_t *task_new_recv(int fd, size_t buf_len)
{
	recv_data_t *data = umalloc(sizeof(recv_data_t));
	data->buf_len = buf_len;
	data->recvd_len = 0;
	data->buf = umalloc(buf_len);

	task_t *task = umalloc(sizeof(task_t));
	task->fd = fd;
	task->op = OP_RECV;
	task->data = data;
	return task;
}

recv_data_t *task_recv_get_data(task_t *task)
{
	assert(task->op == OP_RECV);
	assert(task->data != NULL);
	return task->data;
}

// copy msg to internal buf field
task_t *task_new_send(int fd, uint8_t *msg, size_t len)
{
	send_data_t *data = umalloc(sizeof(send_data_t));
	data->sended_len = 0;
	data->buf = umalloc(len);
	memcpy(data->buf, msg, len);
	data->buf_len = len;

	task_t *task = umalloc(sizeof(task_t));
	task->op = OP_SEND;
	task->fd = fd;
	task->data = data;

	return task;
}

send_data_t *task_send_get_data(task_t *task)
{
	assert(task->op == OP_SEND);
	assert(task->data != NULL);
	return task->data;
}

task_t *task_new_close(int fd)
{
	task_t *task = umalloc(sizeof(task_t));
	task->fd = fd;
	task->data = NULL;
	task->op = OP_CLOSE;
	return task;
}

void task_free(task_t *task)
{
	if (task->op == OP_RECV) {
		recv_data_t *data = task_recv_get_data(task);
		ufree(data->buf);
		ufree(data);
	}
	if (task->op == OP_SEND) {
		send_data_t *data = task_send_get_data(task);
		ufree(data->buf);
		ufree(data);
	}
	task->op = -1;
	task->fd = -1;
	task->data = NULL;
	ufree(task);
}

void create_submission(struct io_uring *ring, task_t *task)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	if (!sqe) {
		int submit_wait = io_uring_submit_and_wait(ring, 1);
		assert(submit_wait > 0);
		sqe = io_uring_get_sqe(ring);
		assert(sqe != NULL);
	}

	switch (task->op) {
	case OP_ACCEPT: {
		io_uring_prep_multishot_accept(sqe, task->fd, NULL, NULL, 0);
		break;
	}

	case OP_RECV: {
		recv_data_t *data = task_recv_get_data(task);
		io_uring_prep_recv(sqe, task->fd, data->buf + data->recvd_len,
				   data->buf_len - data->recvd_len, 0);
		break;
	}

	case OP_SEND: {
		send_data_t *data = task_send_get_data(task);
		io_uring_prep_send(sqe, task->fd, data->buf + data->sended_len,
				   data->buf_len - data->sended_len, 0);
		break;
	}

	case OP_CLOSE: {
		io_uring_prep_close(sqe, task->fd);
		break;
	}
	} // end switch

	io_uring_sqe_set_data(sqe, task);
}
