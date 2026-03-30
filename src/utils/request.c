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
        return true;
    } else if (*bytes_read == 1 && (*start)[0] == '\r') {
        *bytes_read = read(client_socket, buffer + 1, READ_SIZE - 2);
        if (*bytes_read <= 0 || buffer[1] == '\n') {
            return true;
        }
        buffer[0] = '\r';
        *start = buffer;
        (*bytes_read)++;
    } else if (*bytes_read == 0) {
        *bytes_read = read(client_socket, buffer, READ_SIZE - 1);
        if (*bytes_read <= 0 || (*bytes_read >= 2 && buffer[0] == '\r' && buffer[1] == '\n')) {
            return true;
        }
        *start = buffer;
    }
    return false;
}


Http_Request* init_request() {
    Http_Request* req_line = safe_malloc(sizeof(Http_Request));
    req_line->method = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    req_line->target = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    req_line->version = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
    req_line->headers_map = NULL;
    return req_line;
}

void free_request(Http_Request* req_line) {
    free(req_line->method);
    free(req_line->target);
    free(req_line->version);
    if (req_line->headers_map != NULL) {
        for (int i = 0; i < shlen(req_line->headers_map); i++) {
            free(req_line->headers_map[i].key);
            free(req_line->headers_map[i].value);
        }
        shfree(req_line->headers_map);
    }
    free(req_line);
}


Http_Request* parse_request(int client_socket) {
    char buffer[READ_SIZE];
    char* start = buffer;
    char *headerkey, *headerval;
    int bytes_read, method_len, target_len, version_len, headerkey_len, headerval_len;
    bool found_method, found_target, found_version, found_crlf_split, found_headerkey, found_headerval, found_ending;
    
    method_len = target_len = version_len = 0;
    found_method = found_target = found_version = found_crlf_split = found_headerkey = found_headerval = found_ending = false;
    Http_Request* req_line = init_request();

    while (!found_method && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0) {
        read_chunk(&req_line->method, &method_len, " ", &bytes_read, &start, buffer, &found_method);
    }

    do {
        read_chunk(&req_line->target, &target_len, " ", &bytes_read, &start, buffer, &found_target);
    } while (!found_target && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);

    do {
        read_chunk_split(&req_line->version, &version_len, &bytes_read, &start, buffer, &found_crlf_split, &found_version);
    } while (!found_version && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);

    while (!found_crlf(client_socket, &start, buffer, &bytes_read)) {
        headerkey = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
        headerval = safe_malloc(sizeof(char) * INIT_LINE_SIZE);
        headerkey_len = headerval_len = 0;

        do {
            read_chunk(&headerkey, &headerkey_len, ": ", &bytes_read, &start, buffer, &found_headerkey);
        } while (!found_headerkey && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);
        if (!found_headerkey) {
            free(headerkey);
            free(headerval);
            break;
        }

        do {
            read_chunk_split(&headerval, &headerval_len, &bytes_read, &start, buffer, &found_crlf_split, &found_headerval);
        } while (!found_headerval && (bytes_read = read(client_socket, buffer, READ_SIZE - 1)) > 0);
        if (!found_headerval) {
            free(headerkey);
            free(headerval);
            break;
        }

        shput(req_line->headers_map, headerkey, headerval);
    }

    return req_line;
}

