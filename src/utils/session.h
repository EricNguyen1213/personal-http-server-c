#ifndef SESSION_H
#define SESSION_H

#define PORT 4221
#define MAX_PENDING_CONNECTIONS 5
#define MAX_WORKERS 4
#define MAX_EVENTS 10

#include "tools/thpool.h"
#include "request.h"
#include "response.h"

typedef struct {
    int epoll_fd;
    int socket;
} arg_fds;

arg_fds* init_args(int epoll_fd, int socket);

int init_server(int port, int* epoll_fd, int* server_fd, threadpool* pool);
int connect_client(int server_fd, int epoll_fd);
void handle_client(void* arg);

#endif