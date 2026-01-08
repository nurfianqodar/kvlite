#include "server.h"
#include <stdlib.h>

int main()
{
	server_t server;
	int ret = server_init(&server, "127.0.0.1", 8000);
	if (ret != 0) {
		return EXIT_FAILURE;
	}
	ret = server_start(&server);

	if (ret != 0) {
		server_deinit(&server);
		return EXIT_FAILURE;
	}
	server_deinit(&server);
	return EXIT_SUCCESS;
}
