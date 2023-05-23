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
    std::stringstream open_service(std::stringstream& ss);
    std::stringstream close_service(std::stringstream& ss);
    std::stringstream read_service(std::stringstream& ss);
    std::stringstream write_service(std::stringstream& ss);
    std::stringstream lseek_service(std::stringstream& ss);
    std::stringstream create_service(std::stringstream& ss);
    std::stringstream rm_service(std::stringstream& ss);
    std::stringstream ls_service(std::stringstream& ss);
    std::stringstream mkdir_service(std::stringstream& ss);
    std::stringstream cd_service(std::stringstream& ss);
    std::stringstream cat_service(std::stringstream& ss);
    std::stringstream upload_service(std::stringstream& ss);
    std::stringstream download_service(std::stringstream& ss);
    std::string get_error_msg(int error_code);
    
public:
    static std::map<std::string,std::stringstream (Services::*)(std::stringstream&)> command_service_map;
    static std::stringstream process(const std::string& command,std::stringstream &ss,int& code);
    static Services& Instance();
};