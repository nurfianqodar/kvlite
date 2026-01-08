#include "server.h"
#include "task.h"
#include <asm-generic/socket.h>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static volatile sig_atomic_t stop = 0;

static void on_sigint(int sig)
{
	(void)sig;
	stop = 1;
}

static int activate_on_sigint()
{
	struct sigaction sa;
	sa.sa_handler = on_sigint;
	sa.sa_restorer = NULL;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	int ret = sigaction(SIGINT, &sa, NULL);
	if (ret != 0) {
		fprintf(stderr, "activate on sigint error\n");
		return -1;
	}
	return 0;
}

// return false if error and mainloop should stoped
static bool on_cqe(server_t *s, struct io_uring_cqe *cqe)
{
	task_t *task = io_uring_cqe_get_data(cqe);
	switch (task->op) {
	case OP_ACCEPT: {
		if (cqe->res < 0) {
			task_free(task);
			return false;
		}
		task_t *recv_task = task_new_recv(cqe->res, 8192);
		create_submission(&s->ring, recv_task);
		break;
	}

	case OP_CLOSE: {
		fprintf(stdout, "closed fd = %d\n", task->fd);
		task_free(task);
		break;
	}

	case OP_RECV: {
		if (cqe->res <= 0) {
			task_t *close_task = task_new_close(task->fd);
			create_submission(&s->ring, close_task);
			task_free(task);
			break;
		}

		recv_data_t *recv_data = task_recv_get_data(task);
		recv_data->recvd_len += cqe->res;
		task_t *send_task = task_new_send(task->fd, recv_data->buf,
						  recv_data->recvd_len);
		create_submission(&s->ring, send_task);
		task_free(task);
		break;
	}
	case OP_SEND: {
		if (cqe->res < 0) {
			task_t *close_task = task_new_close(task->fd);
			create_submission(&s->ring, close_task);
			task_free(task);
			break;
		}
		send_data_t *send_data = task_send_get_data(task);
		send_data->sended_len += cqe->res;

		if (send_data->sended_len < send_data->buf_len) {
			create_submission(&s->ring, task);
			break;
		}
		task_t *recv_task = task_new_recv(task->fd, 8192);
		create_submission(&s->ring, recv_task);
		task_free(task);
	}
	} // end switch
	return true;
}

int server_init(server_t *s, const char *host, uint16_t port)
{
	int ret = io_uring_queue_init(128, &s->ring, 0);
	if (ret != 0) {
		fprintf(stderr, "io uring init error\n");
		return ret;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	ret = inet_aton(host, &addr.sin_addr);
	if (ret == 0) {
		io_uring_queue_exit(&s->ring);
		fprintf(stderr, "invalid hostname\n");
		return -1;
	}

	s->fd = socket(AF_INET, SOCK_CLOEXEC | SOCK_STREAM, 0);
	if (s->fd < 0) {
		io_uring_queue_exit(&s->ring);
		fprintf(stderr, "socket error\n");
		return s->fd;
	}

	int opt = 1;
	ret = setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
	if (ret != 0) {
		close(s->fd);
		io_uring_queue_exit(&s->ring);
		fprintf(stderr, "setsockopt error\n");
		return ret;
	}

	ret = bind(s->fd, (struct sockaddr *)&addr, sizeof addr);
	if (ret != 0) {
		close(s->fd);
		io_uring_queue_exit(&s->ring);
		fprintf(stderr, "bind error\n");
		return ret;
	}

	ret = listen(s->fd, SOMAXCONN);
	if (ret != 0) {
		close(s->fd);
		io_uring_queue_exit(&s->ring);
		fprintf(stderr, "listen error\n");
		return ret;
	}

	return 0;
}

int server_start(server_t *s)
{
	int ret = activate_on_sigint();
	if (ret != 0) {
		return -1;
	}

	task_t *task_acc = task_new_accept(s->fd);
	create_submission(&s->ring, task_acc);

	while (!stop) {
		int submited = io_uring_submit(&s->ring);
		if (submited < 0) {
			fprintf(stderr, "submit error\n");
			task_free(task_acc);
			return -1;
		}

		struct io_uring_cqe *cqe;
		ret = io_uring_wait_cqe(&s->ring, &cqe);
		if (ret != 0) {
			if (ret != -EINTR) {
				fprintf(stderr, "wait cqe error\n");
				task_free(task_acc);
				return -1;
			}
			task_free(task_acc);
			return 0;
		}
		if (!on_cqe(s, cqe)) {
			task_free(task_acc);
			return -1;
		}
		io_uring_cqe_seen(&s->ring, cqe);
	}
	task_free(task_acc);
	return 0;
}

void server_deinit(server_t *s)
{
	printf("cleaning server\n");
	struct io_uring_cqe *cqe;
	unsigned head;
	unsigned i = 0;
	io_uring_for_each_cqe(&s->ring, head, cqe)
	{
		task_t *task = io_uring_cqe_get_data(cqe);
		task_free(task);
		i++;
	}
	io_uring_cq_advance(&s->ring, i);
	io_uring_queue_exit(&s->ring);
	close(s->fd);
}
