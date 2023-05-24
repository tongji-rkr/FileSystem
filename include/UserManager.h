#ifndef USERMG_H
#define USERMG_H
#define NOERROR 0
typedef int ErrorCode;
#include "FileManager.h"
#include "User.h"
#include "File.h"
#include "INode.h"
#include <iostream>
#include <string.h>
#include <map>
using namespace std;

class UserManager
{
public:
    static const int USER_N = 100;
    UserManager();
    ~UserManager();
    // User Login
    bool Login(string uname);
    // User Logout
    bool Logout();
    // Get User Structure of the current thread
    User* GetUser();

public:
    // 一个动态的索引表
    std::map<pthread_t, int> user_addr;
    // User array
    User* pusers[USER_N]; 
};

#endif
