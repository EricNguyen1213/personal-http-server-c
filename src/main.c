#include "utils/session.h"
#include "utils/tools/thpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>

int main(int argc, char *argv[]) {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	// printf("Logs from your program will appear here!\n");

	// Create Socket File Descriptor
	threadpool pool;
	int epoll_fd, server_fd;
	
	if (init_server(PORT, &epoll_fd, &server_fd, &pool) != 0) {
		return 1;
	}

	struct epoll_event events[MAX_EVENTS];
	while (true) {
		int ready_len = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		for (int i = 0; i < ready_len; i++) {
			if (events[i].data.fd != server_fd) {
				arg_fds* args = init_args(epoll_fd, events[i].data.fd, argc, argv);
				thpool_add_work(pool, (void*) handle_client, (void*) args);
				continue;
			} 

			if (connect_client(server_fd, epoll_fd) != 0) {
				return 1;
			}
		}
	}

	close(server_fd);
	thpool_destroy(pool);
	return 0;
}
