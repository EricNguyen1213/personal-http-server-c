#ifndef RESPONSE_H
#define RESPONSE_H

char* create_status_line(int code);
char* create_text_body(char* body, char* content);
char* create_text_header(char* header, int content_len);
char* create_response(char* status_line, char* headers, char* body);
#endif
