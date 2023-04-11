#include "Kernel.h"
#include "User.h"
#include"string.h"
#include"string"
FD Kernel::Sys_Open(std::string& fpath,int mode)
{
    //模仿系统调用，将参数放入user结构中
    User& u = Kernel::Instance().GetUser();
    char path[256];
    strcpy(path,fpath.c_str());
	u.u_dirp = path;
    //u.u_arg[0] = (int)path;
    u.u_arg[1] = mode;

    FileManager& fileMgr = Kernel::Instance().GetFileManager();
	fileMgr.Open();

    //从user结构取出返回值
	return u.u_ar0[User::EAX];	

}
int Kernel::Sys_Close(FD fd)
{
    User& u = Kernel::Instance().GetUser();
    u.u_arg[0] = fd;

    FileManager& fileMgr = Kernel::Instance().GetFileManager();
	fileMgr.Close();

    return u.u_ar0[User::EAX];
}
int Kernel::Sys_CreatDir(std::string &fpath)
{
    int default_mode = 040755;
    User &u = Kernel::Instance().GetUser();
    u.u_error = NOERROR;
    char filename_char[300]={0};
    strcpy(filename_char, fpath.c_str());
    u.u_dirp = filename_char;
    u.u_arg[1] = default_mode;
    u.u_arg[2] = 0;
    FileManager &fimanag = Kernel::Instance().GetFileManager();
    fimanag.MkNod();
    return u.u_ar0[User::EAX];
}
int Kernel::Sys_Creat(std::string &fpath,int mode)
{
    //模仿系统调用，将参数放入user结构中
    User& u = Kernel::Instance().GetUser();
    char path[256];
    strcpy(path,fpath.c_str());
	u.u_dirp = path;
    u.u_arg[0] = (long long)&path;
    u.u_arg[1] = mode;

    FileManager& fileMgr = Kernel::Instance().GetFileManager();
	fileMgr.Creat();

    //从user结构取出返回值
	return u.u_ar0[User::EAX];	
}
int Kernel::Sys_Delete(std::string &fpath)
{
    //模仿系统调用，将参数放入user结构中
    User& u = Kernel::Instance().GetUser();
    char path[256];
    strcpy(path,fpath.c_str());
	u.u_dirp = path;
    u.u_arg[0] = (long long)&path;

    FileManager& fileMgr = Kernel::Instance().GetFileManager();
	fileMgr.UnLink();

    //从user结构取出返回值
	return u.u_ar0[User::EAX];	
}
int Kernel::Sys_Read(FD fd, size_t size, size_t nmemb, void *ptr)
{
    if(size>nmemb)return -1;
    //模仿系统调用，将参数放入user结构中
    User& u = Kernel::Instance().GetUser();

    u.u_arg[0] = fd;
    u.u_arg[1] = (long long)ptr;
    u.u_arg[2] = size;

    FileManager& fileMgr = Kernel::Instance().GetFileManager();
	fileMgr.Read();

    //从user结构取出返回值
	return u.u_ar0[User::EAX];	

}
int Kernel::Sys_Write(FD fd, size_t size, size_t nmemb, void *ptr)
{
    if(size>nmemb)return -1;
    //模仿系统调用，将参数放入user结构中
    User& u = Kernel::Instance().GetUser();

    u.u_arg[0] = fd;
    u.u_arg[1] = (unsigned long long)ptr;
    u.u_arg[2] = size;

    FileManager& fileMgr = Kernel::Instance().GetFileManager();
	fileMgr.Write();

    //从user结构取出返回值
	return u.u_ar0[User::EAX];	
}
int Kernel::Sys_Seek(FD fd, long int offset, int whence)
{
    //模仿系统调用，将参数放入user结构中
    User& u = Kernel::Instance().GetUser();

    u.u_arg[0] = fd;
    u.u_arg[1] = offset;
    u.u_arg[2] = whence;

    FileManager& fileMgr = Kernel::Instance().GetFileManager();
	fileMgr.Seek();

    //从user结构取出返回值
	return u.u_ar0[User::EAX];	
}
