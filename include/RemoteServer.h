#pragma once

#include "ServerInterface.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <map>
#include <algorithm>
#include <cstring>

#define PORT 8888
#define BACKLOG 128
#define MAX_PACKAGE_LENGTH 1024

class RemoteServer : public ServerInterface
{
public:
    RemoteServer();
    ~RemoteServer();

    int send_message(const std::string &msg, int id);
    int receive_message(int id,std::string&message);

    void bind_process_function(void *(*process_function)(void *) = NULL)
    {
        this->process_function = process_function;
    }

    int run();
    void stop();

    int wait_for_connection();

private:
    struct sigaction action;
    int listen_fd = 0;
    std::map<pthread_t, int *> client_fd_map;
    int sin_size;
    void *(*process_function)(void *);
};
