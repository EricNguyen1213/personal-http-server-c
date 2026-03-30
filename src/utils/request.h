#ifndef REQUEST_H
#define REQUEST_H

#define READ_SIZE 1024
#define INIT_LINE_SIZE 10

#include "tools/stb_ds.h"

typedef struct {
    char* key;
    char* value;
} Header;

typedef struct {
    char* method;
    char* target;
    char* version;
    char* body;
    Header* headers_map;
} Http_Request;


void free_request(Http_Request* req_line);
Http_Request* parse_request(int client_socket, bool* must_close_socket);
char* create_status_line(int code);
#endif
