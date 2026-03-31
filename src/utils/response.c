#include "response.h"
#include "tools/alloc.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <zlib.h>

char* compress_to_gzip(char* body, int* out_size) {
    int body_size = strlen(body);
    char input_buffer[body_size];
    memcpy(input_buffer, body, body_size);
    unsigned long safe_size = compressBound(body_size) + 18;
    char* output = safe_realloc(body, safe_size);
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return NULL;
    }

    zs.next_in = (Bytef *) input_buffer;
    zs.avail_in = (uInt) body_size;
    zs.next_out = (Bytef *) output;
    zs.avail_out = (uInt) safe_size;

    deflate(&zs, Z_FINISH);
    deflateEnd(&zs);

    *out_size = (int) zs.total_out;

    return output;
}

bool has_valid_encoding(char* encoding) {
    if (encoding == NULL) {
        return false;
    }
    if (strcmp("gzip", encoding) == 0) {
        return true;
    }
    if (strstr(encoding, "gzip, ") != NULL || strstr(encoding, " gzip,") != NULL || strstr(encoding, " gzip\0") != NULL) {
        return true;
    }
    return false;
}

char* create_status_line(char** status_line, int code) {
    char *status;
    switch (code) {
        case 200:
            status = "OK";
            break;
        case 201:
            status = "Created";
            break;
        case 404:
            status = "Not Found";
            break;
        default:
            printf("Unknown Status Code");
            return NULL;
    }
    int len = snprintf(NULL, 0, "HTTP/1.1 %d %s", code, status);
    *status_line = safe_realloc(*status_line, len + 1);
    snprintf(*status_line, len + 1, "HTTP/1.1 %d %s", code, status);
    return *status_line;
}

char* create_text_body(char* body, char* content) {
    char* fin_content = content;
    if (content == NULL) {
        fin_content = "";
    }

    int body_len = strlen(content);
    body = safe_realloc(body, body_len + 1);
    snprintf(body, body_len + 1, "%s", content);
    return body;
}

char* create_content_header(bool has_encoding, char* header, char* content_type, int content_len) {
    if (has_encoding) {
        int header_len = snprintf(NULL, 0, "Content-Encoding: gzip\r\nContent-Type: %s\r\nContent-Length: %d\r\n", content_type, content_len);
        header = safe_realloc(header, header_len + 1);
        snprintf(header, header_len + 1, "Content-Encoding: gzip\r\nContent-Type: %s\r\nContent-Length: %d\r\n", content_type, content_len);
        return header;
    } 

    int header_len = snprintf(NULL, 0, "Content-Type: %s\r\nContent-Length: %d\r\n", content_type, content_len - 1);
    header = safe_realloc(header, header_len + 1);
    snprintf(header, header_len + 1, "Content-Type: %s\r\nContent-Length: %d\r\n", content_type, content_len - 1);
    return header;
}

char* create_response(char* status_line, char* headers, char* body, int body_size, int* response_size) {
    int before_body_len = strlen(status_line) + strlen(headers) + 4;
    *response_size = before_body_len + body_size;
    char* response = safe_malloc(*response_size);
    snprintf(response, before_body_len + 1, "%s\r\n%s\r\n", status_line, headers);
    memcpy(response + before_body_len, body, body_size);
    return response;
}

char* create_file_body(char* body, FILE* f) {
    size_t body_len = 0;
    size_t bytes_read;
    char buffer[READ_SIZE];
    while ((bytes_read = fread(buffer, 1, READ_SIZE, f)) > 0) {
        body = safe_realloc(body, body_len + bytes_read + 1);
        memcpy(body + body_len, buffer, bytes_read);
        body_len += bytes_read;
    }
    body[body_len] = '\0';
    return body;
}

char* compress_body(bool has_encoding, char* body, int* body_size) {
    char* fin_body = body;
    *body_size = strlen(body) + 1;
    if (has_encoding) {
        fin_body = compress_to_gzip(body, body_size);
    }
    return fin_body;
}

void handle_gets(char* dir, Http_Request* request, char** status_line, char** headers, char** body, int* body_size) {
    bool has_encoding = has_valid_encoding(shget(request->headers_map, "Accept-Encoding"));
    *status_line = create_status_line(status_line, 404);

    if (strcmp("/", request->target) == 0) {
        *status_line = create_status_line(status_line, 200);

    } else if (strncmp("/echo/", request->target, 6) == 0) {
        *status_line = create_status_line(status_line, 200);
        *body = create_text_body(*body, request->target + 6);
        *body = compress_body(has_encoding, *body, body_size);
        *headers = create_content_header(has_encoding, *headers, "text/plain", *body_size);

    } else if (strcmp("/user-agent", request->target) == 0) {
        *status_line = create_status_line(status_line, 200);
        *body = create_text_body(*body, shget(request->headers_map, "User-Agent"));
        *body = compress_body(has_encoding, *body, body_size);
        *headers = create_content_header(has_encoding, *headers, "text/plain", *body_size);

    } else if (strncmp("/files/", request->target, 7) == 0) {
        bool has_encoding = has_valid_encoding(shget(request->headers_map, "Accept-Encoding"));
        char* filename = request->target + 7;
        if (dir != NULL) {
            int path_len = strlen(dir) + strlen(filename);
            char fullpath[path_len + 1];
            snprintf(fullpath, path_len + 1, "%s%s", dir, filename);

            FILE* file = fopen(fullpath, "r");
            if (file != NULL) {
                *status_line = create_status_line(status_line, 200);
                *body = create_file_body(*body, file);
                *body = compress_body(has_encoding, *body, body_size);
                *headers = *headers = create_content_header(has_encoding, *headers, "application/octet-stream", *body_size);
                fclose(file);
            } 

        }
    
    }
}

void handle_posts(char* dir, Http_Request* request, char** status_line, char** headers, char** body, int* body_size) {
    *status_line = create_status_line(status_line, 404);
    if (strncmp("/files/", request->target, 7) == 0) {
        char* filename = request->target + 7;
        if (dir != NULL) {
            int path_len = strlen(dir) + strlen(filename);
            char fullpath[path_len + 1];
            snprintf(fullpath, path_len + 1, "%s%s", dir, filename);

            FILE* file = fopen(fullpath, "w");
            if (file != NULL) {
                fputs(request->body, file);
                *status_line = create_status_line(status_line, 201);
                fclose(file);
            }

        }

    }
}
