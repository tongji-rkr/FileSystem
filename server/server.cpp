#include "Kernel.h"
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>

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
#include "RemoteServer.h"
#include "Services.h"
#include "Kernel.h"
#define PORT 8888
#define BACKLOG 128

RemoteServer server;

using namespace std;

void handle_pipe(int sig)
{
    // 不做任何处理即可
}

bool isNumber(const string &str)
{
    for (char const &c : str)
    {
        if (std::isdigit(c) == 0)
            return false;
    }
    return true;
}

stringstream print_head()
{
    stringstream send_str;
    // send_str << "===============================================" << endl;
    // send_str << "||请在一行中依次输入需要调用的函数名称及其参数  ||" << endl;
    // send_str << "||open(char *name, int mode)                 ||" << endl;
    // send_str << "||close(int fd)                              ||" << endl;
    // send_str << "||read(int fd, int length)                   ||" << endl;
    // send_str << "||write(int fd, char *buffer, int length)    ||" << endl;
    // send_str << "||seek(int fd, int position, int ptrname)    ||" << endl;
    // send_str << "||mkfile(char *name, int mode)               ||" << endl;
    // send_str << "||rm(char *name)                             ||" << endl;
    // send_str << "||ls()                                       ||" << endl;
    // send_str << "||mkdir(char* dirname)                       ||" << endl;
    // send_str << "||cd(char* dirname)                          ||" << endl;
    // send_str << "||cat(char* dirname)                         ||" << endl;
    // send_str << "||copyin(char* ofpath, char *  ifpath)       ||" << endl;
    // send_str << "||copyout(char* ifpath, char *  ofpath)      ||" << endl;
    // send_str << "||q/Q 退出文件系统                           ||" << endl
    //          << endl
    //          << endl;
    send_str <<"---------------------------------------------------------------"<<endl;
    send_str << ">>>help menu<<<" << endl;
    send_str << "open [filename] [mode] : to open a file with selected mode" << endl;
    send_str << "close [fd] : to close a file with selected fd" << endl;
    send_str << "read [fd] [length] : to read the file with selected fd" << endl;
    send_str << "write [fd] [text] : to write text into the file with selected fd" << endl;
    send_str << "lseek [fd] [position] [ptrname] : to seek the file with selected fd" << endl;
    send_str << "create [filename] : to create a file" << endl;
    send_str << "rm [filename] : to remove a file" << endl;
    send_str << "ls : to list all files in current directory" << endl;
    send_str << "mkdir [dirname] : to create a directory" << endl;
    send_str << "cd [dirname] : to change current directory" << endl;
    send_str << "cat [filename] : to print the content of a file" << endl;
    send_str << "upload [localpath] [remotepath] : to upload a file from local to remote" << endl;
    send_str << "download [remotepath] [localpath] : to download a file from remote to local" << endl;
    send_str << "q/Q : to quit the file system" << endl;
    send_str <<"---------------------------------------------------------------"<<endl;


    return send_str;
}
class sendU
{
private:
    int fd;
    string username;

public:
    int send_(const stringstream &send_str)
    {
        // cout<<send_str.str()<<endl;
        int numbytes = send(fd, send_str.str().c_str(), send_str.str().length(), 0);
        cout << "[[ " << username << " ]] send " << numbytes << " bytes" << endl;
        cout << "====message send====" << endl;
        cout << send_str.str() << endl;
        cout << "====================" << endl;
        return numbytes;
    };
    sendU(int fd, string username)
    {
        this->fd = fd;
        this->username = username;
    };
};

void *start_routine(void *ptr)
{
    int fd = *(int *)ptr;
    char buf[1024];
    int numbytes;
    // numbytes=send(fd,"请输入用户名：",sizeof("请输入用户名："),0);
    numbytes = server.send_message("please type in the username:", fd);
    if (numbytes == -1)
    {
        cout << "send() error" << endl;
        return (void *)NULL;
    }

    printf("进入用户线程，fd=%d\n", fd);

    memset(buf, 0, sizeof(buf));
    if ((numbytes = recv(fd, buf, 1024, 0)) == -1)
    {
        cout << ("recv() error\n");
        return (void *)NULL;
    }

    string username = buf;
    cout << "[info] 用户输入用户名：" << username << endl;

    sendU sd(fd, username);

    // 初始化用户User结构和目录
    Kernel::Instance().GetUserManager().Login(username);
    // init the prompt
    stringstream welcome_str = print_head();
    string tipswords = username + "@FileSystem:" + Kernel::Instance().GetUserManager().GetUser()->u_curdir + "$";

    // send the welcome message
    welcome_str << tipswords;
    sd.send_(welcome_str);
    bool first_output = true;
    while (true)
    {
        char buf_recv[1024] = {0};

        // 读取用户输入的命令行
        if ((numbytes = recv(fd, buf_recv, 1024, 0)) == -1)
        {
            cout << "recv() error" << endl;
            Kernel::Instance().GetUserManager().Logout();
            return (void *)NULL;
        }
        // 解析命令名称
        stringstream ss(buf_recv);
        stringstream send_str;

        cout << "buf_recv : " << buf_recv << endl;
        string api;
        ss >> api;

        cout << "api : " << api << endl;
        
        if (api == "q" || api == "quit")
        {
            Kernel::Instance().GetUserManager().Logout();
            send_str << "user logout\n";
            sd.send_(send_str);
            break;
        }

        int code;
        send_str = Services::process(api, ss, code);
        if (code)
        {
            cout << "unrecognized command!" << endl;
            send_str = print_head();
            send_str << "\n"
                     << "wrong command!\n";
        }
        else
        {
            cout << "command finished!" << endl;
        }

        // show the prompt with username and current directory
        string tipswords = username + "@FileSystem:" + Kernel::Instance().GetUserManager().GetUser()->u_curdir + "$";

        send_str << tipswords;

        // 发送提示
        numbytes = sd.send_(send_str);
        if (numbytes <= 0)
        {
            cout << "[NETWORK] user " << username << " disconnect." << endl;
            Kernel::Instance().GetUserManager().Logout();
            return (void *)NULL;
        }
        printf("[NETWORK] send %d bytes\n", numbytes);
    }

    close(fd);
    return (void *)NULL;
}

// int main()
// {

//     // 进行信号处理
//     struct sigaction action;
//     action.sa_handler = handle_pipe;
//     sigemptyset(&action.sa_mask);
//     action.sa_flags = 0;
//     sigaction(SIGPIPE, &action, NULL);

//     int listenfd, connectfd;
//     struct sockaddr_in server;
//     struct sockaddr_in client;
//     int sin_size;
//     sin_size=sizeof(struct sockaddr_in);

//     // 创建监听fd
//     if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
//         perror("Creating socket failed.");
//         exit(1);
//     }

//     int opt = SO_REUSEADDR;
//     setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //使得端口释放后立马被复用
//     bzero(&server,sizeof(server));
//     server.sin_family=AF_INET;
//     server.sin_port=htons(PORT);
//     server.sin_addr.s_addr = htonl (INADDR_ANY);
//     // 绑定
//     if (bind(listenfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
//         perror("Bind error.");
//         exit(1);
//     }
//     // 监听
//     if(listen(listenfd,BACKLOG) == -1){  /* calls listen() */
//         perror("listen() error\n");
//         exit(1);
//     }
//     // 初始化文件系统
//     Kernel::Instance().Initialize();
//     cout << "[info] 等待用户接入..." << endl;
//     // 进入通信循环
//     while(1){
//         // accept
//         if ((connectfd = accept(listenfd,(struct sockaddr *)&client, (socklen_t*)&sin_size))==-1) {
//             perror("accept() error\n");
//             continue;
//         }
//         printf("客户端接入：%s\n",inet_ntoa(client.sin_addr) );
//         string str="hello";
//         //send(connectfd,str.c_str(),6,0);
//         pthread_t thread; //定义一个线程号
//         pthread_create(&thread,NULL,start_routine,(void *)&connectfd);
//     }
//     close(listenfd);
//     return 0;
// }

int main()
{
    // init the server
    server.bind_process_function(start_routine);
    server.run();
    // init the file system
    Kernel::Instance().Initialize();
    while (true)
    {
        // get the connection
        if (-1 == server.wait_for_connection())
        {
            cout << "[ERROR] wait for connection failed" << endl;
            continue;
        }
    }
    return 0;
}
