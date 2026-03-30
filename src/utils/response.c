#include "response.h"
#include "tools/alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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

char* create_text_header(char* header, int content_len) {
    int header_len = snprintf(NULL, 0, "Content-Type: text/plain\r\nContent-Length: %d\r\n", content_len);
    header = safe_realloc(header, header_len + 1);
    snprintf(header, header_len + 1, "Content-Type: text/plain\r\nContent-Length: %d\r\n", content_len);
    return header;
}

char* create_response(char* status_line, char* headers, char* body) {
    int resp_len = strlen(status_line) + strlen(headers) + strlen(body) + 4;
    char* response = safe_malloc(resp_len + 1);
    snprintf(response, resp_len + 1, "%s\r\n%s\r\n%s", status_line, headers, body);
    return response;
}
