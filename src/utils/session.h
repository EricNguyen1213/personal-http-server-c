#ifndef SESSION_H
#define SESSION_H

#define PORT 4221
#define MAX_PENDING_CONNECTIONS 5
#define READ_SIZE 1024
#define INIT_LINE_SIZE 10

#define RESPONSE_200 "HTTP/1.1 200 OK\r\n\r\n"
#define RESPONSE_404 "HTTP/1.1 404 Not Found\r\n\r\n"

typedef struct {
    char* method;
    char* target;
    char* version;
} http_request_line;


int init_server(int port, int* server_fd);
int connect_client(int server_fd, int* client_socket);
char* handle_request(int client_socket);
int send_response(int client_socket, char* response, int size);

http_request_line* init_request_line();
void free_request_line(http_request_line* req_line);
http_request_line* parse_request_line(int client_socket);


#endif