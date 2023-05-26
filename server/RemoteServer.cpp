#include "RemoteServer.h"

using namespace std;

RemoteServer::RemoteServer()
{
    sin_size=sizeof(struct sockaddr_in); 
}

RemoteServer::~RemoteServer()
{

}

int RemoteServer::run()
{
    // check whether the process function is set
    if (this->process_function == NULL)
    {
        cout << "[NETWORK] process function is not set" << endl;
        return -1;
    }

    // 进行信号处理
    struct sigaction action;
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGPIPE, &action, NULL);

    struct sockaddr_in server;    
    

    // 创建监听fd
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {   
        perror("Creating socket failed.");
        exit(1);
    }

    int opt = SO_REUSEADDR;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //使得端口释放后立马被复用
    bzero(&server,sizeof(server));  
    server.sin_family=AF_INET; 
    server.sin_port=htons(PORT); 
    server.sin_addr.s_addr = htonl (INADDR_ANY); 
    // 绑定
    if (bind(listen_fd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) { 
        perror("Bind error.");
        status=DISCONNECTED;
        return -1;
    }
    // 监听 
    if(listen(listen_fd,BACKLOG) == -1){  /* calls listen() */ 
        perror("listen() error\n"); 
        status=DISCONNECTED;
        return -1;
    }
    
    cout<<"[NETWORK] server built successfully"<<endl;
    return 0;
}

void RemoteServer::stop()
{
    // release the resource
    for (auto it = client_fd_map.begin(); it != client_fd_map.end(); it++)
    {
        close(*it->second);
        delete it->second;
    }
    // close the socket
    close(listen_fd);
}

int RemoteServer::send_message(const std::string &msg,int id)
{
    int bytes_sent = send(id, msg.c_str(), msg.size(), 0);
    if (bytes_sent == -1)
    {
        cout << "[NETWORK] send error, error code " << errno << "(" << strerror(errno) << ")" << endl;
        return -1;
    }
    if (bytes_sent > 0)
    {
        cout << "[NETWORK] send message to client of " << bytes_sent << " bytes" << endl;
        
        // print the message
        cout << "======================Message sent====================="<< endl;
        cout << msg << endl;
        cout << "======================================================="<< endl;
    }
    return 0;
}

int RemoteServer::receive_message(int id,string& message)
{
    char buf[MAX_PACKAGE_LENGTH];
    int bytes_received = recv(id, buf, MAX_PACKAGE_LENGTH, 0);
    if (bytes_received == -1)
    {
        cout << "[NETWORK] receive error, error code " << errno << "(" << strerror(errno) << ")" << endl;
        return -1;
    }
    if (bytes_received > 0)
    {
        cout << "[NETWORK] receive message from client of " << bytes_received << " bytes" << endl;
        
        // print the message
        cout << "======================Message recv====================="<< endl;
        cout << buf << endl;
        cout << "======================================================="<< endl;
        message = buf;
    }
    return 0;
}

int RemoteServer::wait_for_connection()
{
    int connectfd;
    struct sockaddr_in client;  
    // accept 
    if ((connectfd = accept(listen_fd,(struct sockaddr *)&client, (socklen_t*)&sin_size))==-1) {
        perror("accept() error\n"); 
        return -1;
    }
    cout<<"[NETWORK] server: got connection from "<<inet_ntoa(client.sin_addr)<<":"<<ntohs(client.sin_port)<<endl;
    pthread_t thread_id;
    client_fd_map[thread_id] = new int;
    *client_fd_map[thread_id] = connectfd;
    pthread_create(&thread_id, NULL,process_function, (void*)&connectfd);

    return 0;
}