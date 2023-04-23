// this file implements the remote client class which is a subclass of ClientInterface

#include "RemoteClient.h"

using namespace std;

RemoteClient::RemoteClient()
{
    this->fd = 0;
    bzero(&this->server_addr, sizeof(this->server_addr));
}

RemoteClient::~RemoteClient()
{
}

void RemoteClient::send_message(const std::string &msg)
{
    // check the status
    if (this->status == DISCONNECTED)
    {
        cout << "[NETWORK] client is not connected" << endl;
        return;
    }

    // filter out the empty message
    if (msg.length() == 0)
    {
        cout << "[NETWORK] empty message" << endl;
        return;
    }

    string message = msg;
    // ignore the \n on the end of the message
    if (msg[msg.length()-1] == '\n')
    {
        message = msg.substr(0, msg.length()-1);
    }

    // send the message
    int numbytes = send(this->fd, message.c_str(), message.length(), 0);
    if (numbytes == -1)
    {
        cout << "[NETWORK] send error, error code " << errno << "(" << strerror(errno) << ")" << endl;
        this->status = DISCONNECTED;
        return;
    }
    if (numbytes > 0)
    {
        cout << "[NETWORK] send message to server of " << numbytes << " bytes" << endl;
        
        // print the message
        // cout << "======================Message out======================="<< endl;
        // cout << message << endl;
        // cout << "======================================================="<< endl;
    }
}

void RemoteClient::receive_message()
{
    // check the status
    if (this->status == DISCONNECTED)
    {
        cout << "[NETWORK] client is not connected" << endl;
        return;
    }

    // receive the message
    int received_size = recv(this->fd, this->receive_buffer, MAX_PACKAGE_LENGTH, 0);
    if (received_size == 0)
    {
        cout << "[NETWORK] server closed, client quit." << endl;
        this->status = DISCONNECTED;
        return;
    }
    if (received_size == -1)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            cout << "[NETWORK] recv error, error code " << errno << "(" << strerror(errno) << ")" << endl;
            this->status = DISCONNECTED;
            return;
        }
        if (errno == EINTR)
        {
            // no message received, do nothing
            return;
        }
        cout<<"[NETWORK] no message received"<<endl;
    }
    if (received_size > 0)
    {
        cout << "[NETWORK] receive message from server of " << received_size << " bytes" << endl;

        // print the message
        // cout << "======================Message in======================="<< endl;
        // cout << this->receive_buffer << endl;
        // cout << "======================================================="<< endl;

        this->receive_buffer[received_size] = 0;
        // cout << this->receive_buffer;
    }

    // call the callback function
    this->recv_callback(this->receive_buffer);
}

int RemoteClient::run(){
    return this->run("127.0.0.1",8888);
}

int RemoteClient::run(const std::string &ip, const unsigned int port)
{

    // check the callback function is binded
    if (this->recv_callback == nullptr)
    {
        cout << "[NETWORK] recv callback is not binded" << endl;
        return -1;
    }

    // create socket
    this->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->fd < 0)
    {
        cout << "[NETWORK] create socket failed" << endl;
        return -1;
    }

    // setup the server address
    this->server_addr.sin_family = AF_INET;
    this->server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &this->server_addr.sin_addr);

    // set the socket to non-blocking
    // int flags = fcntl(this->fd, F_GETFL, 0);
    // flags |= O_NONBLOCK;
    // if (fcntl(this->fd, F_SETFL, flags) < 0)
    // {
    //     cout << "[NETWORK] set flags error" << endl;
    //     close(this->fd);
    //     return -1;
    // }

    // try to connect the server
    int cnt = 1;
    while (true)
    {
        int rc = connect(this->fd, (struct sockaddr *)&this->server_addr, sizeof(this->server_addr));
        // successfull connected
        if (rc == 0)
        {
            cout << "[NETWORK] connect to server successfully." << endl;
            break;
        }
        if (cnt > TIME_OUT)
        {
            cout << "[NETWORK] connect failed, client quit." << endl;
            return -1;
        }
        cnt++;
        cout << "[NETWORK] try to connect to server, cnt: " << cnt << endl;
        sleep(1);
    }
    // update the client satatus
    this->status = CONNECTED;
    return 0;
}

void RemoteClient::stop()
{
    close(this->fd);
    this->status = DISCONNECTED;
    cout << "[NETWORK] client quit." << endl;
}