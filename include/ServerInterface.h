// this file is the interface for the server

#pragma once

#include<iostream>
#include<cstring>
#include<string>

class ServerInterface{
public:
    virtual int run()=0;
    virtual void stop()=0;
    virtual int send_message(const std::string &msg,int id)=0;
    virtual int receive_message(int id,std::string& message)=0;
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