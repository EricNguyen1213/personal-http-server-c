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
#include <fcntl.h>
#include <sys/epoll.h>


int set_nonblocking(int fd) {
    // 1. Get the current status flags
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return 1;
    }

    // 2. Set the flags back with O_NONBLOCK enabled
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return 1;
    }

    return 0;
}



arg_fds* init_args(int epoll_fd, int socket, int argc, char** argv) {
    arg_fds* args = safe_malloc(sizeof(arg_fds));
    args->epoll_fd = epoll_fd;
    args->socket = socket;
    args->dir = NULL;

    if (argc <= 1) {
        return args;
    }

    if (strcmp(argv[1], "--directory") == 0) {
        args->dir = argv[2];
    }
    return args;
}


int init_server(int port, int* epoll_fd, int* server_fd, threadpool* pool) {
    
    // Create Socket File Descriptor
	*server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
    if (set_nonblocking(*server_fd) != 0) {
        printf("Setting Server FD Nonblocking failed: %s...\n", strerror(errno));
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

    *pool = thpool_init(MAX_WORKERS);

    *epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (*epoll_fd == -1) {
        printf("Epoll FD failed: %s \n", strerror(errno));
        return 1;
    }

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = *server_fd;

    if (epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, *server_fd, &event) == -1) {
        printf("Epoll Socket Fail To Add: %s \n", strerror(errno));
        return 1;
    }
    return 0;
}


int connect_client(int server_fd, int epoll_fd) {
    printf("Waiting for a client to connect...\n");
    struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);
	
    int client_socket = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	if (client_socket < 0) {
        printf("Accepting Client Connection failed: %s \n", strerror(errno));
        return 1;
    }
    if (set_nonblocking(client_socket) != 0) {
        printf("Setting Client Socket Nonblocking failed: %s...\n", strerror(errno));
		return 1;
    }

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    event.data.fd = client_socket;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &event) == -1) {
        printf("Epoll Socket Fail To Add: %s \n", strerror(errno));
        return 1;
    }

	printf("Client connected\n");
    return 0;
}


char* handle_request(Http_Request* request, arg_fds* args) {
    char *response, *status_line;
    char* headers = strdup("");
    char* body = strdup("");

    if (strcmp("/", request->target) == 0) {
        status_line = create_status_line(200);

    } else if (strncmp("/echo/", request->target, 6) == 0) {
        status_line = create_status_line(200);
        body = create_text_body(body, request->target + 6);
        headers = create_content_header(headers, "text/plain", strlen(body));

    } else if (strcmp("/user-agent", request->target) == 0) {
        status_line = create_status_line(200);
        body = create_text_body(body, shget(request->headers_map, "User-Agent"));
        headers = create_content_header(headers, "text/plain", strlen(body));

    } else if (strncmp("/files/", request->target, 7) == 0) {
        char* filename = request->target + 7;
        if (args->dir != NULL) {
            int path_len = strlen(args->dir) + strlen(filename);
            char fullpath[path_len + 1];
            snprintf(fullpath, path_len + 1, "%s%s", args->dir, filename);

            FILE* file = fopen(fullpath, "r");
            if (file != NULL) {
                status_line = create_status_line(200);
                body = create_file_body(body, file);
                headers = create_content_header(headers, "application/octet-stream", strlen(body));
                fclose(file);
            } else {
                status_line = create_status_line(404);
            }

        } else {
            status_line = create_status_line(404);
        }
    
    } else {
        status_line = create_status_line(404);
    }

    response = create_response(status_line, headers, body);

    // printf("%s\n", request->method);
    // printf("%s\n", request->version);

    free(status_line);
    free(headers);
    free(body);
    free_request(request);
    return response;
}


int send_response(int client_socket, char* response) {
    int already_sent = 0;
    int response_size = strlen(response);
    while (already_sent < response_size) {
        int sent = send(client_socket, response + already_sent, response_size - already_sent, 0);
        if (sent < 0) {
            printf("Sending Response failed: %s \n", strerror(errno));
            break;
        }
        already_sent += sent;
    }
    free(response);
    return already_sent;
}

void handle_client(void* args) {
    arg_fds* argfds = (arg_fds*) args;
    bool must_close_socket = false;
    Http_Request* request = parse_request(argfds->socket, &must_close_socket);
    if (request != NULL) {
        char* response = handle_request(request, argfds);
        send_response(argfds->socket, response);
    }

    if (must_close_socket) {
        close(argfds->socket);
    } else {
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT; 
        event.data.fd = argfds->socket;

        if (epoll_ctl(argfds->epoll_fd, EPOLL_CTL_MOD, argfds->socket, &event) == -1) {
            // If this fails, the socket might be closed; clean up
            perror("epoll_ctl: mod");
            close(argfds->socket);
        }
    }
    free(argfds);
}



