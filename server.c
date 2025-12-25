#include "server.h"
#include "io.h"
#include <asm-generic/socket.h>
#include <liburing.h>
#include <signal.h>
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

int server_init(server_t *server, const char *host, size_t host_len,
		uint16_t port)
{
	int ret;

	if (host_len > 46) {
		goto ret_err;
	}
	memcpy(server->host, host, host_len);
	server->host[host_len] = '\0';
	server->port = port;

	ret = io_init(&server->io, 2048, 512);
	if (ret < 0)
		goto ret_err;

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

	struct sigaction sa;
	sa.sa_handler = on_sigint;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	ret = listen(server->fd, SOMAXCONN);
	if (ret < 0)
		return -1;

	ret = io_submit_accept_multishot(&server->io, server->fd);
	ret = io_submit_accept_multishot(&server->io, server->fd);
	ret = io_submit_accept_multishot(&server->io, server->fd);
	if (ret < 0)
		return -1;

	while (!stop) {
		int ret;
		struct io_uring_cqe *cqe;
		io_data_t *io_data;

		ret = io_submit(&server->io);
		if (ret < 0) {
			break;
		}

		printf("submited %d\n", ret);

		ret = io_uring_wait_cqe(&server->io.ring, &cqe);
		if (ret < 0) {
			if (ret == -EINTR && stop)
				break;
			continue;
		}

		io_data = io_get_data(cqe);

		switch (io_data->op) {
		case OP_UNSPECIFIED:
		case OP_CLOSE:
		case OP_CANCEL: {
			break;
		}
		case OP_ACCEPT: {
			int clientfd = cqe->res;
			if (clientfd < 0)
				break;
			ret = io_submit_recv_multishot(&server->io, clientfd);
			if (ret < 0) {
				perror("recv multishot error\n");
				printf("client fd = %d", clientfd);
				close(clientfd);
				// break;
				return -1;
			}
			break;
		}
		case OP_RECV: {
			ssize_t readn;
			int clientfd;
			uint8_t *buffer;

			readn = io_get_recv_result_len(cqe);
			printf("readed %lu bytes\n", readn);
			clientfd = io_data->fd;

			if (readn <= 0) {
				io_submit_close(&server->io, clientfd);
			}
			buffer = io_get_recv_result(&server->io, cqe);
			io_submit_send(&server->io, clientfd, buffer, readn);
			break;
		}
		case OP_SEND: {
			int bid = IO_BGID_READ;
			uint8_t *buffer =
				server->io.buffer_pool + (bid * IO_BUF_LEN);
			io_uring_buf_ring_add(server->io.br, buffer, IO_BUF_LEN,
					      bid,
					      io_uring_buf_ring_mask(IO_NRBUFS),
					      bid);
			io_uring_buf_ring_advance(server->io.br, 1);
			io_free_data(&server->io, cqe);
			break;
		}
		} // end switch
		io_uring_cqe_seen(&server->io.ring, cqe);

	} // end loop
	return 0;
}
