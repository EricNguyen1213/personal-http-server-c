#include "response.h"
#include "tools/alloc.h"
#include <stdlib.h>
#include <string.h>


char* create_status_line(int code) {
    char *status, *status_line;
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
    status_line = safe_malloc(len + 1);
    snprintf(status_line, len + 1, "HTTP/1.1 %d %s", code, status);
    return status_line;
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

char* create_content_header(char* header, char* content_type, int content_len) {
    int header_len = snprintf(NULL, 0, "Content-Type: %s\r\nContent-Length: %d\r\n", content_type, content_len);
    header = safe_realloc(header, header_len + 1);
    snprintf(header, header_len + 1, "Content-Type: %s\r\nContent-Length: %d\r\n", content_type, content_len);
    return header;
}

char* create_response(char* status_line, char* headers, char* body) {
    int resp_len = strlen(status_line) + strlen(headers) + strlen(body) + 4;
    char* response = safe_malloc(resp_len + 1);
    snprintf(response, resp_len + 1, "%s\r\n%s\r\n%s", status_line, headers, body);
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


void handle_gets(char* dir, Http_Request* request, char** status_line, char** headers, char** body) {
    if (strcmp("/", request->target) == 0) {
        *status_line = create_status_line(200);

    } else if (strncmp("/echo/", request->target, 6) == 0) {
        *status_line = create_status_line(200);
        *body = create_text_body(*body, request->target + 6);
        *headers = create_content_header(*headers, "text/plain", strlen(*body));

    } else if (strcmp("/user-agent", request->target) == 0) {
        *status_line = create_status_line(200);
        *body = create_text_body(*body, shget(request->headers_map, "User-Agent"));
        *headers = create_content_header(*headers, "text/plain", strlen(*body));

    } else if (strncmp("/files/", request->target, 7) == 0) {
        char* filename = request->target + 7;
        if (dir != NULL) {
            int path_len = strlen(dir) + strlen(filename);
            char fullpath[path_len + 1];
            snprintf(fullpath, path_len + 1, "%s%s", dir, filename);

            FILE* file = fopen(fullpath, "r");
            if (file != NULL) {
                *status_line = create_status_line(200);
                *body = create_file_body(*body, file);
                *headers = create_content_header(*headers, "application/octet-stream", strlen(*body));
                fclose(file);
            } else {
                *status_line = create_status_line(404);
            }

        } else {
            *status_line = create_status_line(404);
        }
    
    } else {
        *status_line = create_status_line(404);
    }
}

void handle_posts(char* dir, Http_Request* request, char** status_line, char** headers, char** body) {
    if (strncmp("/files/", request->target, 7) == 0) {
        char* filename = request->target + 7;
        if (dir != NULL) {
            int path_len = strlen(dir) + strlen(filename);
            char fullpath[path_len + 1];
            snprintf(fullpath, path_len + 1, "%s%s", dir, filename);

            FILE* file = fopen(fullpath, "w");
            if (file != NULL) {
                fputs(request->body, file);
                *status_line = create_status_line(201);
                fclose(file);
            } else {
                *status_line = create_status_line(404);
            }

        } else {
            *status_line = create_status_line(404);
        }
    
    } else {
        *status_line = create_status_line(404);
    }
}
