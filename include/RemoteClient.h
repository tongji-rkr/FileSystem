// this file implements the remote client class which is a subclass of ClientInterface
#pragma once

#include "ClientInterface.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string>
#include <cstring>
#include <iostream>
#include <strings.h>

#define MAX_PACKAGE_LENGTH 1024*16 // the max length of a package
#define TIME_OUT 3 // the time out of the socket

class RemoteClient : public ClientInterface
{
public:
    RemoteClient();
    ~RemoteClient();

    void send_message(const std::string &msg);
    void receive_message();

    int run();
    int run(const std::string & ip, const unsigned int port);
    void stop();

private:
    int fd;
    struct sockaddr_in server_addr;
    char receive_buffer[MAX_PACKAGE_LENGTH] = {0};
    char send_buffer[MAX_PACKAGE_LENGTH] = {0};

};
