#include "session.h"
#include "tools/alloc.h"
#include "tools/stb_ds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>


int init_server(int port, int* server_fd) {
    
    // Create Socket File Descriptor
	*server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse_opt = 1;
	if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_opt, sizeof(reuse_opt)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	// Configure server address structure
	struct sockaddr_in serv_addr = {.sin_family = AF_INET, .sin_port = htons(port), .sin_addr = { htonl(INADDR_ANY) }};

	// Bind the socket to the port
	if (bind(*server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

    // Listen for incoming connections
	if (listen(*server_fd, MAX_PENDING_CONNECTIONS) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

    return 0;
}


int connect_client(int server_fd, int* client_socket) {
    printf("Waiting for a client to connect...\n");
    struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);
	
    *client_socket = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	if (*client_socket < 0) {
        printf("Accepting Client Connection failed: %s \n", strerror(errno));
        return 1;
    }
	printf("Client connected\n");
    return 0;
}



char* handle_request(int client_socket) {
    char *response, *status_line;
    char* headers = strdup("");
    char* body = strdup("");
    Http_Request* request = parse_request(client_socket);

    if (strcmp("/", request->target) == 0) {
        status_line = create_status_line(200);

    } else if (strncmp("/echo/", request->target, 6) == 0) {
        status_line = create_status_line(200);
        body = create_text_body(body, request->target + 6);
        headers = create_text_header(headers, strlen(body));

    } else if (strcmp("/user-agent", request->target) == 0) {
        status_line = create_status_line(200);
        body = create_text_body(body, shget(request->headers_map, "User-Agent"));
        headers = create_text_header(headers, strlen(body));

    } else {
        status_line = create_status_line(404);
    }

    response = create_response(status_line, headers, body);

    // printf("%s\n", request->method);
    // printf("%s\n", request->target);
    // printf("%s\n", request->version);

    free(status_line);
    free(headers);
    free(body);
    free_request(request);
    return response;
}


int send_response(int client_socket, char* response, int size) {
    // printf("herenow\n");
    // printf("%s\n", response);
    int already_sent = 0;
    while (already_sent < size) {
        int sent = send(client_socket, response + already_sent, size - already_sent, 0);
        if (sent < 0) {
            printf("Sending Response failed: %s \n", strerror(errno));
            break;
        }
        already_sent += sent;
    }
    free(response);
    return already_sent;
}



