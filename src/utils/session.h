#ifndef SESSION_H
#define SESSION_H

#define PORT 4221
#define MAX_PENDING_CONNECTIONS 5

#include "request.h"


int init_server(int port, int* server_fd);
int connect_client(int server_fd, int* client_socket);
char* handle_request(int client_socket);
int send_response(int client_socket, char* response, int size);

#endif