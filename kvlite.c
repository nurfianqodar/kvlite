#include "server.h"
int main()
{
	server_t s;

	server_init(&s, "127.0.0.1", 9, 8000);
	server_start(&s);
	return 0;
}
