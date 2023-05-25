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
#include <sstream>
#include <vector>

#include "cJSON.h"
#include "RemoteClient.h"
#include "base64.h"
#define MAX_FILE_SIZE 1024


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
static bool last_down_load = false;
static string last_down_load_filename = "";

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

// 定义一个函数，用于接收用户指令
string get_command()
{
  //   cout<<"getting command"<<endl;
  // 定义一个字符串变量，用于存储用户指令
  string command = "";
  // 定义一个字符变量，用于存储用户输入的字符
  char ch;
  // 循环读取用户输入的字符，直到遇到换行符为止
  while ((ch = cin.get()) != '\n')
  {
    // 如果用户输入的是退格键
    if (ch == '\b')
    {
      // 如果指令不为空
      if (!command.empty())
      {
        // 删除指令最后一个字符
        command.pop_back();
        // 输出一个空格和一个退格键，用于清除屏幕上的字符
        cout << " \b";
      }
    }
    // 否则
    else
    {
      // 将用户输入的字符追加到指令中
      command += ch;
      // 输出用户输入的字符
      cout << ch;
    }
  }
  // 返回用户指令
  return command;
}

string split_files(){


}

int get_file_length(std::string path, int& fd){
    //打开文件
    fd = open(path.c_str(), O_RDWR);
    if (fd == -1) {
        perror("open");
        return -1;
    }
    //获取文件大小
    int fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize == -1) {
        perror("lseek");
        return -1;
    }
    return fileSize;
}

string get_file_string(int fd, int fileSize, int offset){
    //读取文件内容
    char *buf = new char[fileSize];
    lseek(fd, offset, SEEK_SET);
    int ret = read(fd, buf, fileSize);
    string raw_str=string(buf, fileSize);
    string str_base64 = base64_encode(reinterpret_cast<const unsigned char*>(raw_str.c_str()), raw_str.length());
    return str_base64;
}

bool process_upload(const string & upload_command, vector<string>& content){
    // parse the command and check whether the parameter is correct and the file exists
    // format: upload <filename_local> <filename_server>
    stringstream ss(upload_command);
    string command;
    string filename_local;
    string filename_server;
    ss>>command>>filename_local>>filename_server;
    if(filename_local.empty()||filename_server.empty()){
        cout<<"parameter error"<<endl;
        return false;
    }
    // // open the file and read content to string
    // FILE *fp = fopen(filename_local.c_str(),"r");
    // if(fp==NULL){
    //     cout<<endl<<"file not exists"<<endl;
    //     return false;
    // }
    // char ch;
    // while((ch=fgetc(fp))!=EOF){
    //     content+=ch;
    // }
    int fd = 0;
    int fileSize= get_file_length(filename_local, fd);
    if(fileSize==-1){
        cout<<"file not exists"<<endl;
        return false;
    } 
    //分包
    int offset = 0;
    while(fileSize > 0)
    {
        int packetSize = fileSize > MAX_FILE_SIZE ? MAX_FILE_SIZE : fileSize;   
        string str_base64 = get_file_string(fd, packetSize, offset);
        content.push_back(str_base64);
        fileSize -= MAX_FILE_SIZE;
        offset += MAX_FILE_SIZE;
    }

    return true;

}

string get_file_content(const string & filename){
    string content;
    FILE *fp = fopen(filename.c_str(),"r");
    if(fp==NULL){
        cout<<"file not exists"<<endl;
        return content;
    }
    char ch;
    while((ch=fgetc(fp))!=EOF){
        content+=ch;
    }
    return content;
}



void receive_message_handler(const string& message)
{
    // record whether the last command is download


    // cout<<message<<endl;

    // cout<<message;
    cJSON *display = cJSON_Parse(message.c_str());


    if(last_down_load){
        // write the content to the file
        cout<<"trying to open file:"<<last_down_load_filename<<endl;
        int fd = open(last_down_load_filename.c_str(), O_RDWR|O_CREAT, 0666);
        cJSON* file_content = cJSON_GetObjectItem(display, "content");
        if(file_content==NULL){
            cout<<"[ERROR] file content not exists"<<endl;
            return;
        }
        string file_content_str = file_content->valuestring;
        // cout<<file_content_str<<endl;
        string file_content_str_decode = base64_decode(file_content_str);
        int ret = write(fd, file_content_str_decode.c_str(), file_content_str_decode.size());
        if(ret==-1){
            cout<<"[ERROR] write file failed"<<endl;
            return;
        }
        cout<<"[INFO] download file successfully"<<endl;
        last_down_load = false;
    }

    // get the command
    cJSON* display_message = cJSON_GetObjectItem(display, "command");
    if(display_message==NULL){
        return;
    }
    string display_message_str = display_message->valuestring;
    cout<<display_message_str;
    

    string send_message;
    // getline(cin, send_message);

    send_message = get_command();
    

    if (send_message.empty())
    {
        send_message = " ";
    }

    // prepare the message to send
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "command", send_message.c_str());
    
    
    stringstream ss(send_message);
    string cmd, p1_ofpath, p2_ifpath;
    ss >> cmd >> p1_ofpath >> p2_ifpath;

    // checke whether the command is "upload"
    // open the file and read content to string

    vector<string> content;
    
    if(send_message.substr(0,6)=="upload"){
        bool result = process_upload(send_message, content);
        cout<<endl<<"doing upload"<<endl;
        if(!result){
            // to send a message to server to get a response again
            // cJSON *root_new = cJSON_CreateObject();
            // cJSON_AddStringToObject(root_new, "command", "upload failed");
            // char *final_message = cJSON_Print(root_new);
            // string final_message_str(final_message);
            // // send the message to server
            // client.send_message(final_message_str);
            cJSON *root_new = cJSON_CreateObject();
            printf("upload failed\n");
            cJSON_AddStringToObject(root_new, "command", " ");
            char *final_message = cJSON_Print(root_new);
            string final_message_str(final_message);
            // send the message to server
            client.send_message(final_message_str);
            return;
        }
        cJSON_AddStringToObject(root, "fileNum", to_string(content.size()).c_str());
        char *final_message = cJSON_Print(root);
        string final_message_str(final_message);
        // send the message to server
        client.send_message(final_message_str);
        cout <<"send json ---"<<endl << final_message_str << endl;  
        for(int i=0;i<content.size();i++){
            string str_base64 = content[i];
            string name = p2_ifpath.substr(0, p2_ifpath.find_last_of(".")) + "_" + to_string(i) + p2_ifpath.substr(p2_ifpath.find_last_of("."));
            cJSON *root_new = cJSON_CreateObject();
            cJSON_AddStringToObject(root_new, "command", "upload");
            cJSON_AddStringToObject(root_new, "filename", name.c_str());
            cJSON_AddStringToObject(root_new, "totalNum", to_string(content.size()).c_str());
            cJSON_AddStringToObject(root_new, "remainNum", to_string(content.size()-i-1).c_str());
            cJSON_AddStringToObject(root_new, "content", str_base64.c_str());
            char *final_message = cJSON_Print(root_new);
            string final_message_str(final_message);
            // send the message to server
            client.send_message(final_message_str);
            cout <<"send json ---"<<endl << final_message_str << endl;
            sleep(1);
        }
        return;
    }
    
    // check whether the command is "download"
    if(send_message.substr(0,8)=="download"){
        // get the filename
        stringstream ss(send_message);
        string command;
        string filename_remote,filename_local;
        ss>>command>>filename_remote>>filename_local;
        if(filename_local.empty()){
            cout<<"parameter error"<<endl;
            cJSON *root_new = cJSON_CreateObject();
            cJSON_AddStringToObject(root_new, "command", "download failed");
            char *final_message = cJSON_Print(root_new);
            string final_message_str(final_message);
            return;
        }
        // record the filename
        last_down_load_filename = filename_local;
        last_down_load = true;
    }

    // check whether it is q or Q
    if (send_message == "q" || send_message == "Q")
    {
        client.stop();
    }
    
    char *final_message = cJSON_Print(root);
    string final_message_str(final_message);
    cout<<"message sent is "<<endl<<final_message_str<<endl;
    // send the message to server
    client.send_message(final_message_str);

    
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