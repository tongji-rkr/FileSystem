// this file defines an interface for the client

#pragma once

#include <iostream>

class ClientInterface
{
public:
    virtual int run() = 0; // return 0 if success and -1 if failed
    virtual void stop() = 0;
    virtual void send_message(const std::string &msg) = 0;
    virtual void receive_message() = 0;
    virtual ~ClientInterface() {}

    void bind_recv_callback(void (*callback)(const std::string &))
    {
        recv_callback = callback;
    }

    bool is_closed()
    {
        return status == DISCONNECTED;
    }
    bool is_connected()
    {
        return status == CONNECTED;
    }
protected:
    void (*recv_callback)(const std::string &);
    enum {CONNECTED, DISCONNECTED} status=DISCONNECTED;
};