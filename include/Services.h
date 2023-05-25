// this file defines the services that the server provides

#include<iostream>
#include<map>
#include<string>
#include<cstring>
#include<sstream>
#include<unistd.h>
#include<fcntl.h>
#include"User.h"
#include"Kernel.h"
#include"FileManager.h"
#include"Utility.h"
#include"cJSON.h"



// class Services{
// private:
//     static std::map<std::string,std::stringstream (Services::*)(std::stringstream&)> command_service_map;
//     Services(){}


// public:


//     Services(const Services&)=delete;
//     Services& operator=(const Services&)=delete;

//     std::stringstream cd_hook(std::stringstream& ss);

//     static std::stringstream process(const std::string& command,std::stringstream &ss);
// };

class Services{
private:
    Services(){}
    
    // service hook function
    std::stringstream open_service(std::stringstream& ss,cJSON* root);
    std::stringstream close_service(std::stringstream& ss,cJSON* root);
    std::stringstream read_service(std::stringstream& ss,cJSON* root);
    std::stringstream write_service(std::stringstream& ss,cJSON* root);
    std::stringstream lseek_service(std::stringstream& ss,cJSON* root);
    std::stringstream create_service(std::stringstream& ss,cJSON* root);
    std::stringstream rm_service(std::stringstream& ss,cJSON* root);
    std::stringstream ls_service(std::stringstream& ss,cJSON* root);
    std::stringstream mkdir_service(std::stringstream& ss,cJSON* root);
    std::stringstream cd_service(std::stringstream& ss,cJSON* root);
    std::stringstream cat_service(std::stringstream& ss,cJSON* root);
    std::stringstream upload_service(std::stringstream& ss,cJSON* root);
    std::stringstream download_service(std::stringstream& ss,cJSON* root);
    std::string get_error_msg(int error_code);
    
public:
    static std::map<std::string,std::stringstream (Services::*)(std::stringstream&,cJSON*)> command_service_map;
    static std::stringstream process(const std::string& command,std::stringstream &ss,int& code,cJSON* root);
    static Services& Instance();
};