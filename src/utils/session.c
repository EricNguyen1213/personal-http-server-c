#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <glib.h>




void* safe_malloc(size_t size) {
  void* ptr = malloc(size);
  if (ptr == NULL) {
    printf("Failed to Allocate: %s \n", strerror(errno));
    exit(1);
  }
  return ptr;
}


void* safe_realloc(void* ptr, size_t size) {
  void* temp = realloc(ptr, size);
  if (temp == NULL) {
    printf("Failed to Reallocate: %s \n", strerror(errno));
    exit(1);
  }
  return temp;
}


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

http_request_line* init_request_line() {
    http_request_line* req_line = safe_malloc(sizeof(http_request_line));
    req_line->method = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    req_line->target = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    req_line->version = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    return req_line;
}

void free_request_line(http_request_line* req_line) {
    free(req_line->method);
    free(req_line->target);
    free(req_line->version);
    free(req_line);
}

void read_chunk(char** line, int* line_len, char* split, int* bytes_read, char** start, char* buffer, bool* found) {
    (*start)[*bytes_read] = '\0';
    char* next_part = strstr(*start, split);
    if (next_part != NULL) {
        int index = (int)(next_part - *start);
        *line = safe_realloc(*line, *line_len + index + 1);
        strlcpy(*line + *line_len, *start, index + 1);
        *line_len += index + 1;
        *bytes_read -= index + strlen(split);
        *start = next_part + strlen(split);
        *found = true;
    } else {
        *line = safe_realloc(*line, *line_len + *bytes_read);
        memcpy(*line + *line_len, *start, *bytes_read);
        *line_len += *bytes_read;
        *start = buffer;
    }
}

void read_chunk_split(char** line, int* line_len, int* bytes_read, char** start, char* buffer, bool* cerf_split, bool* found) {
    (*start)[*bytes_read] = '\0';

    if (*cerf_split && *bytes_read > 0 && (*start)[0] == '\n') {
        *line = safe_realloc(*line, *line_len + 1);
        (*line)[*line_len] = '\0';
        *line_len++;
        *bytes_read -= 1;
        *start = buffer + 1;
        *found = true;
        *cerf_split = false;
        return;
    }
    
    *cerf_split = false;
    char* next_part = strstr(*start, "\r\n");
    int cerf_size = strlen("\r\n");
    if (next_part != NULL) {
        int index = (int)(next_part - *start);
        *line = safe_realloc(*line, *line_len + index + 1);
        strlcpy(*line + *line_len, *start, index + 1);
        *line_len += index + 1;
        *bytes_read -= index + cerf_size;
        *start = next_part + cerf_size;
        *found = true;
        return;
    }
    
    if ((*start)[*bytes_read - 1] == '\r') {
        *cerf_split = true;
    }

    *line = safe_realloc(*line, *line_len + *bytes_read);
    memcpy(*line + *line_len, *start, *bytes_read);
    *line_len += *bytes_read;
    *start = buffer;
}


http_request_line* parse_request_line(int client_socket) {
    char buffer[READ_SIZE];
    char* start = buffer;
    int bytes_read, method_len, target_len, version_len;
    bool found_method, found_target, found_version, found_crlf_split;
    
    method_len = target_len = version_len = 0;
    found_method = found_target = found_version = found_crlf_split = false;
    http_request_line* req_line = init_request_line();

    while (!found_method && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0) {
        read_chunk(&req_line->method, &method_len, " ", &bytes_read, &start, buffer, &found_method);
    }

    do {
        read_chunk(&req_line->target, &target_len, " ", &bytes_read, &start, buffer, &found_target);
    } while (!found_target && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);

    do {
        read_chunk_split(&req_line->version, &version_len, &bytes_read, &start, buffer, &found_crlf_split, &found_version);
    } while (!found_version && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);

    return req_line;
}

char* create_status_line(int code) {
    char *status, *status_line;
    switch (code) {
        case 200:
            status = "OK";
            break;
        case 404:
            status = "Not Found";
            break;
        default:
            printf("Unknown Status Code");
            return NULL;
    }
    int len = snprintf(NULL, 0, "HTTP/1.1 %d %s\r\n", code, status);
    status_line = safe_malloc(len + 1);
    snprintf(status_line, len + 1, "HTTP/1.1 %d %s\r\n", code, status);
    return status_line;
}


char* handle_request(int client_socket) {
    char *response, *status_line, *resp_header, *body;
    resp_header = "";
    body = "";
    http_request_line* req_line = parse_request_line(client_socket);
    if (strcmp(req_line->target, "/") == 0) {
        status_line = create_status_line(200);
    } else {
        status_line = create_status_line(404);
    }

    int resp_len = strlen(status_line) + strlen(resp_header) + strlen(body) + 2;
    response = safe_malloc(resp_len + 1);
    snprintf(response, resp_len + 1, "%s\r\n%s\r\n%s", status_line, resp_header, body);

    printf("%s\n", req_line->method);
    printf("%s\n", req_line->target);
    printf("%s\n", req_line->version);

    free(status_line);
    free_request_line(req_line);
    return response;
}


int send_response(int client_socket, char* response, int size) {
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



