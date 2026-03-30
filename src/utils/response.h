#ifndef RESPONSE_H
#define RESPONSE_H

#include <stdio.h>

#define READ_SIZE 1024


char* create_status_line(int code);
char* create_text_body(char* body, char* content);
char* create_file_body(char* body, FILE* f);
char* create_content_header(char* header, char* content_type, int content_len);
char* create_response(char* status_line, char* headers, char* body);
#endif
