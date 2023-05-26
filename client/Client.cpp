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
#define MAX_FILE_SIZE 960
#define SEND_INTERVAL 1


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
static string download_string = "";

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

int open_file(std::string path, int& fileSize){
    //打开文件
    int fd = open(path.c_str(), O_RDWR);
    if (fd == -1) {
        perror("open");
        return -1;
    }
    printf("open file %s success\n", path.c_str());
    //获取文件大小
    fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize == -1) {
        perror("lseek");
        return -1;
    }
    printf("file size: %d\n", fileSize);
    return fd;
}

void read_file(int fd, int fileSize, char* buf){
    //读取文件内容
    lseek(fd, 0, SEEK_SET);
    int ret = read(fd, buf, fileSize);
    close(fd);
    return;
}

// string split(int fd, int fileSize, int offset){
//     //读取文件内容
//     char *buf = new char[fileSize];
//     lseek(fd, offset, SEEK_SET);
//     int ret = read(fd, buf, fileSize);
//     string raw_str=string(buf, fileSize);
//     // string str_base64 = base64_encode(reinterpret_cast<const unsigned char*>(raw_str.c_str()), raw_str.length());
//     return str_base64;
// }

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
    int fileSize = 0;
    int fd = open_file(filename_local, fileSize);
    char* buf = new char[fileSize];
    read_file(fd, fileSize, buf);
    string str_base64 = base64_encode(reinterpret_cast<const unsigned char*>(buf), fileSize);
    printf("file string: %s\n", str_base64.c_str());
    //分包
    int base64_size = str_base64.size();
    int cnt = 0;
    while(base64_size>0){
        int packetSize = base64_size > MAX_FILE_SIZE ? MAX_FILE_SIZE : base64_size;   
        string packet = str_base64.substr(cnt, packetSize);
        content.push_back(packet);
        base64_size -= packetSize;
        cnt += packetSize;
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


void save_file()
{
    int fd = open(last_down_load_filename.c_str(), O_RDWR | O_CREAT, 0664);
    if (fd == -1)
    {
        perror("open");
        return;
    }
    string file_content_str_decode = base64_decode(download_string);
    int ret = write(fd, file_content_str_decode.c_str(), file_content_str_decode.size());
    if(ret==-1){
        cout<<"[ERROR] write file failed"<<endl;
        return;
    }
    close(fd);
}

void receive_message_handler(const string& message)
{
    // record whether the last command is download

    cout<<"[INFO] receive message"<<endl;
    cout<<message<<endl;

    // cout<<message;
    cJSON *display = cJSON_Parse(message.c_str());


    if(last_down_load){
        // write the content to the file
        string command = cJSON_GetObjectItem(display, "command")->valuestring;
        if(command == "download"){
            // cout<<"[ERROR] download failed"<<endl;
            if(cJSON_HasObjectItem(display, "content")){
                string content = cJSON_GetObjectItem(display, "content")->valuestring;
                string remainNumStr = cJSON_GetObjectItem(display, "remainNum")->valuestring;
                string totalNumStr = cJSON_GetObjectItem(display, "totalNum")->valuestring;
                cout<<"[INFO] total Package:"<<totalNumStr<<endl;
                cout<<"[INFO] remain Package:"<<remainNumStr<<endl;
                int remainNum = stoi(remainNumStr);
                int totalNum = stoi(totalNumStr);
                string file_name = last_down_load_filename.substr(0, last_down_load_filename.find_last_of(".")) + "_"+to_string(totalNum-remainNum)+".tmp";
                cout<<"[INFO] write to file:"<<file_name<<endl;
                int fd = open(file_name.c_str(), O_RDWR|O_CREAT, 0666);
                string file_content_str_decode = content;
                int ret = write(fd, file_content_str_decode.c_str(), file_content_str_decode.size());
                if(ret==-1){
                    cout<<"[ERROR] write file failed"<<endl;
                    return;
                }
                download_string += content;
                close(fd);
                printf("remainNum: %d\n", remainNum);
                if(remainNum == 0)
                {
                    save_file();
                    last_down_load = false;
                } 
                return;
            }
        }
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
            string str = content[i];
            string name = p2_ifpath.substr(0, p2_ifpath.find_last_of(".")) + "_" + to_string(i) + p2_ifpath.substr(p2_ifpath.find_last_of("."));
            cJSON *root_new = cJSON_CreateObject();
            cJSON_AddStringToObject(root_new, "command", "upload");
            cJSON_AddStringToObject(root_new, "filename", name.c_str());
            cJSON_AddStringToObject(root_new, "totalNum", to_string(content.size()).c_str());
            cJSON_AddStringToObject(root_new, "remainNum", to_string(content.size()-i-1).c_str());
            cJSON_AddStringToObject(root_new, "content", str.c_str());
            char *final_message = cJSON_Print(root_new);
            string final_message_str(final_message);
            // send the message to server
            client.send_message(final_message_str);
            cout <<"send json ---"<<endl << final_message_str << endl;
            sleep(SEND_INTERVAL);
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