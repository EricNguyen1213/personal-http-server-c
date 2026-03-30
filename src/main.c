#include "utils/session.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	// printf("Logs from your program will appear here!\n");

	// Create Socket File Descriptor
	int server_fd, client_socket;
	
	if (init_server(PORT, &server_fd) != 0) {
		return 1;
	}

	if (connect_client(server_fd, &client_socket) != 0) {
		return 1;
	}

	char* response = handle_request(client_socket);
	send_response(client_socket, response, strlen(response));
	
	close(client_socket);
	close(server_fd);
	return 0;
}
