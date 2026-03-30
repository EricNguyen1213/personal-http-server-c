#include "request.h"
#include "tools/alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>


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

bool found_crlf(int client_socket, char** start, char* buffer, int* bytes_read) {
    if (*bytes_read >= 2 && (*start)[0] == '\r' && (*start)[1] == '\n') {
        *start += 2;
        return true;
    } else if (*bytes_read == 1 && (*start)[0] == '\r') {
        *bytes_read = read(client_socket, buffer + 1, READ_SIZE - 2);
        if (*bytes_read <= 0 || buffer[1] == '\n') {
            *start = buffer + 2;
            return true;
        }
        buffer[0] = '\r';
        *start = buffer;
        (*bytes_read)++;
    } else if (*bytes_read == 0) {
        *bytes_read = read(client_socket, buffer, READ_SIZE - 1);
        if (*bytes_read <= 0 || (*bytes_read >= 2 && buffer[0] == '\r' && buffer[1] == '\n')) {
            *start = buffer + 2;
            return true;
        }
        *start = buffer;
    }
    return false;
}


Http_Request* init_request() {
    Http_Request* request = safe_malloc(sizeof(Http_Request));
    request->method = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    request->target = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    request->version = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    request->headers_map = NULL;
    request->body = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    return request;
}

void free_request(Http_Request* request) {
    free(request->method);
    free(request->target);
    free(request->version);
    if (request->headers_map != NULL) {
        for (int i = 0; i < shlen(request->headers_map); i++) {
            free(request->headers_map[i].key);
            free(request->headers_map[i].value);
        }
        shfree(request->headers_map);
    }
    free(request->body);
    free(request);
}


Http_Request* parse_request(int client_socket, bool* must_close_socket) {
    char buffer[READ_SIZE];
    char* start = buffer;
    char *headerkey, *headerval;
    int bytes_read, attr_len;
    bool found_attr, found_crlf_split;
    
    found_attr = found_crlf_split = false;
    Http_Request* request = init_request();

    attr_len = 0;
    while (!found_attr && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0) {
        read_chunk(&request->method, &attr_len, " ", &bytes_read, &start, buffer, &found_attr);
    }
    if (bytes_read == 0) {
        free_request(request);
        *must_close_socket = true;
        return NULL;
    }
    // if (!found_method) {
    //     *data = memcpy(*data, req_line->method, strlen(req_line->method));
    //     memcpy(*data + strlen(req_line->method), start, bytes_read);
    //     *data[strlen(req_line->method) + bytes_read] = '\0';
    //     free_request(req_line);
    //     return NULL;
    // }

    attr_len = 0;
    found_attr = false;
    do {
        read_chunk(&request->target, &attr_len, " ", &bytes_read, &start, buffer, &found_attr);
    } while (!found_attr && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);

    attr_len = 0;
    found_attr = false;
    do {
        read_chunk_split(&request->version, &attr_len, &bytes_read, &start, buffer, &found_crlf_split, &found_attr);
    } while (!found_attr && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);

    while (!found_crlf(client_socket, &start, buffer, &bytes_read)) {
        headerkey = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
        headerval = safe_malloc(sizeof(char) * INIT_LINE_SIZE);

        attr_len = 0;
        found_attr = false;
        do {
            read_chunk(&headerkey, &attr_len, ": ", &bytes_read, &start, buffer, &found_attr);
        } while (!found_attr && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);
        if (!found_attr) {
            free(headerkey);
            free(headerval);
            break;
        }

        attr_len = 0;
        found_attr = false;
        do {
            read_chunk_split(&headerval, &attr_len, &bytes_read, &start, buffer, &found_crlf_split, &found_attr);
        } while (!found_attr && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);
        if (!found_attr) {
            free(headerkey);
            free(headerval);
            break;
        }

        shput(request->headers_map, headerkey, headerval);
    }

    attr_len = 0;
    do {
        request->body = safe_realloc(request->body, attr_len + bytes_read + 1);
        memcpy(request->body + attr_len, start, bytes_read);
        attr_len += bytes_read;
        start = buffer;
    } while ((bytes_read = read(client_socket, buffer, READ_SIZE)) >= 0);
    request->body[attr_len] = '\0';

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        printf("Reading From Client Failed: %s...\n", strerror(errno));
        *must_close_socket = true;
        free_request(request);
        return NULL;
    }

    return request;
}

