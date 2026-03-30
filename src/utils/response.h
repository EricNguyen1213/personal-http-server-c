#ifndef RESPONSE_H
#define RESPONSE_H

#include <stdio.h>
#include "request.h"

#define READ_SIZE 1024


char* create_response(char* status_line, char* headers, char* body);
void handle_gets(char* dir, Http_Request* request, char** status_line, char** headers, char** body);
void handle_posts(char* dir, Http_Request* request, char** status_line, char** headers, char** body);
#endif
