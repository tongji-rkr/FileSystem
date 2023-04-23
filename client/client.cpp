#include <cstdio>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string>
#include <iostream>

#include "RemoteClient.h"

#define MAXDATASIZE 1024

using namespace std;

const string welcome_string =
    " _____ _ _      ____            _               \n"
    "|  ___(_) | ___/ ___| _   _ ___| |_ ___ _ __ ___  \n"
    "| |_  | | |/ _ \\___ \\| | | / __| __/ _ \\ '_ ` _ \\  \n"
    "|  _| | | |  __/___) | |_| \\__ \\ ||  __/ | | | | | \n"
    "|_|   |_|_|\\___|____/ \\__, |___/\\__\\___|_| |_| |_| \n"
    "                      |___/                       \n"
    ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";

// create remote client
RemoteClient client;

// void running(int fd)
// {
//     int numbytes;
//     char receiveM[1025] = {0};
//     char sendM[1025] = {0};
//     bool first_recv = 0;

//     while (true)
//     {
//         // receive the message from server
//         while (1)
//         {
//             numbytes = recv(fd, receiveM, MAXDATASIZE, 0);
//             if (numbytes == 0)
//             {
//                 cout << "[NETWORK] server closed, client quit." << endl;
//                 return;
//             }
//             if (numbytes == -1)
//             {
//                 if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
//                 {
//                     cout << "[NETWORK] recv error, error code " << errno << "(" << strerror(errno) << ")" << endl;
//                     return;
//                 }
//                 if (errno == EINTR)
//                     continue;
//                 if (!first_recv)
//                     continue;
//             }
//             if (numbytes > 0)
//             {
//                 // printf("[INFO] revc %d bytes\n", numbytes);
//                 cout << "[INFO] receive message from server of " << numbytes << " bytes" << endl;
//                 receiveM[numbytes] = 0;
//                 cout << receiveM;
//                 if (!first_recv)
//                     first_recv = 1;
//             }
//             break;
//         }

//         // input
//         fgets(sendM, 1024, stdin);
//         int send_le;
//         send_le = strlen(sendM);
//         sendM[send_le - 1] = '\0'; // remove the '\n'
//         if (send_le == 1)
//         {
//             send_le += 1;
//             sendM[0] = ' ';
//         }
//         // send the message to server
//         if ((numbytes = send(fd, sendM, send_le - 1, 0)) == -1)
//         {
//             printf("[NETWORK] send error\n");
//             return;
//         }
//         // printf("[INFO] send %d bytes\n", numbytes);
//     }
// }

// int main(int argc, char **argv)
// {

//     // show welcome string
//     cout << welcome_string << endl;

//     // get the ip and port from the input
//     string ipaddr;
//     cout << "Please input the ip address of the server (default:127.0.0.1): ";
//     getline(cin, ipaddr);
//     if (ipaddr.empty())
//     {
//         ipaddr = "127.0.0.1";
//     }

//     // get the port from the input
//     unsigned int port;
//     cout << "Please input the port of the server (default:8888): ";
//     getline(cin, ipaddr);
//     if (ipaddr.empty())
//     {
//         port = 8888;
//     }
//     else
//     {
//         port = atoi(ipaddr.c_str());
//     }

//     int fd = 0;
//     struct sockaddr_in addr;
//     fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (fd < 0)
//     {
//         fprintf(stderr, "create socket failed,error:%s.\n", strerror(errno));

//         return -1;
//     }

//     bzero(&addr, sizeof(addr));
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(port);
//     inet_pton(AF_INET, ipaddr.c_str(), &addr.sin_addr);

//     /* set the socket to non-blocking */
//     int flags = fcntl(fd, F_GETFL, 0);
//     flags |= O_NONBLOCK;
//     if (fcntl(fd, F_SETFL, flags) < 0)
//     {
//         fprintf(stderr, "Set flags error:%s\n", strerror(errno));
//         close(fd);
//         return -1;
//     }

//     /*try to connect the server*/
//     int cnt = 1;
//     while (true)
//     {
//         int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
//         // successfull connected
//         if (rc == 0)
//         {
//             cout << "[NETWORK] connect to server successfully." << endl;
//             break;
//         }
//         if (cnt > 5)
//         {
//             cout << "[NETWORK] connect failed, client quit." << endl;
//             return 0;
//         }
//         cout << "[NETWORK] try to connect to server for the " << cnt++ << " time." << endl;
//     }

//     running(fd);
//     close(fd);
//     printf("[NETWORK] shutdown the client.\n");
//     return 0;
// }

void receive_message_handler(const string& message)
{
    cout<<message;
    // prepare the message to send
    string send_message;
    getline(cin, send_message);
    if (send_message.empty())
    {
        send_message = " ";
    }

    // send the message to server
    client.send_message(send_message);

    // check whether it is q or Q
    if (send_message == "q" || send_message == "Q")
    {
        client.stop();
    }
}

string get_ip_address(){
    // get the ip and port from the input
    string ipaddr;
    cout << "Please input the ip address of the server (default:127.0.0.1): ";
    getline(cin, ipaddr);
    if (ipaddr.empty())
    {
        ipaddr = "127.0.0.1";
    }
    return ipaddr;
}

unsigned int get_port(){
    // get the port from the input
    string temp;
    unsigned int port;
    cout << "Please input the port of the server (default:8888): ";
    getline(cin, temp);
    if (temp.empty())
    {
        port = 8888;
    }
    else
    {
        port = atoi(temp.c_str());
    }
    return port;
}

int main()
{
    // show welcome string
    cout << welcome_string << endl;

    string ipaddr = get_ip_address();
    unsigned int port = get_port();
    
    // set the callback function
    client.bind_recv_callback(receive_message_handler);

    if(client.run(ipaddr, port)==-1)
    {
        // error occured when running
        return 0;
    }
    
    // service loop
    while(1)
    {
        client.receive_message();
        if(client.is_closed()){
            break;
        }
    }
    return 0;
}