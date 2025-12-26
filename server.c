#include "server.h"
#include "io.h"
#include <asm-generic/socket.h>
#include <liburing.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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

static int _server_bind(server_t *server)
{
	if (server == NULL)
		return -1;

	if (server->fd < 0)
		return -1;

	int ret;
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(server->port);
	ret = inet_aton((char *)server->host, &addr.sin_addr);
	if (ret < 0)
		return -1;
	ret = bind(server->fd, (struct sockaddr *)&addr, sizeof addr);
	if (ret < 0)
		return -1;
	return 0;
}

static void _server_handle_cqe(server_t *server, struct io_uring_cqe *cqe)
{
	io_data_t *io_data = io_get_data(cqe);

	switch (io_data->op) {
	case OP_UNSPECIFIED: {
		stop = 1; // UNDEFINIED BEHAVIOR
		break;
	}

	case OP_CANCEL: {
	}

	case OP_CLOSE: {
		break;
	}

	case OP_ACCEPT: {
		int clfd;
		int ret;

		clfd = cqe->res;
		if (clfd < 0)
			break;

		ret = io_submit_recv_multishot(&server->io, clfd);
		if (ret < 0) {
			io_submit_close(&server->io, clfd);
			break;
		}
	}

	case OP_RECV: {
		int ret;
		ssize_t recvn;
		unsigned short bid;
		uint8_t *recv_buf;
		uint8_t *send_buf;

		recvn = cqe->res;
		bid = io_get_recv_buffer_id(cqe);
		recv_buf = io_get_recv_buffer(&server->io, bid);
		if (recvn <= 0) {
			io_submit_close(&server->io, io_data->fd);
			io_free_data(&server->io, cqe);
			goto cleanup;
		}

		send_buf = malloc(recvn * sizeof(uint8_t));
		if (send_buf == NULL)
			goto cleanup;
		memcpy(send_buf, recv_buf, recvn);
		ret = io_submit_send(&server->io, io_data->fd, send_buf, recvn);
		if (ret < 0) {
			free(send_buf);
		}

cleanup:
		io_recycle_recv_buffer(&server->io, bid);
		break;
	}

	case OP_SEND: {
		uint8_t *send_buf;

		send_buf = io_data->data;
		if (send_buf != NULL)
			free(send_buf);
		break;
	}

	} // end switch
}

int server_init(server_t *server, const char *host, size_t host_len,
		uint16_t port)
{
	// save host as null terminated string
	int ret;
	if (host_len > 46) {
		goto ret_err;
	}
	memcpy(server->host, host, host_len);
	server->host[host_len] = '\0';
	server->port = port;

	// initialize io utils
	ret = io_init(&server->io, 2048, 512, 2, 4096);
	if (ret < 0)
		goto ret_err;

	// create socket fd
	server->fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (server->fd < 0)
		goto io_clear;
	int opt = 1;
	ret = setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &opt,
			 sizeof opt);
	if (ret < 0)
		goto fd_close;
	ret = _server_bind(server);
	if (ret < 0)
		goto fd_close;

	return 0;

fd_close:
	close(server->fd);
io_clear:
	io_free(&server->io);
ret_err:
	return -1;
}

int server_start(server_t *server)
{
	int ret;

	if (server == NULL)
		return -1;
	if (server->fd < 0)
		return -1;

	// add signal handler
	struct sigaction sa;
	sa.sa_handler = on_sigint;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	ret = listen(server->fd, SOMAXCONN);
	if (ret < 0)
		return -1;

	ret = io_submit_accept_multishot(&server->io, server->fd);
	if (ret < 0)
		return -1;

	while (!stop) {
		int ret;
		struct io_uring_cqe *cqe;

		ret = io_uring_submit(&server->io.ring);
		if (ret < 0)
			break;

		ret = io_uring_wait_cqe(&server->io.ring, &cqe);
		if (ret < 0) {
			if (ret == -EINTR && stop)
				break;
			continue;
		}

		_server_handle_cqe(server, cqe);

		io_uring_cqe_seen(&server->io.ring, cqe);

	} // end loop
	return 0;
}
