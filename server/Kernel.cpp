#include "Kernel.h"
#include <string.h>
#include <assert.h>
// 全局单体实例
Kernel Kernel::instance;

DiskDriver g_DiskDriver;
BufferManager g_BufferManager;
FileSystem g_FileSystem;
FileManager g_FileManager;
User g_User;

char returnBuf[256];

// 构建与析构
Kernel::Kernel()
{
}
Kernel::~Kernel()
{
}


// 初始化
void Kernel::Initialize()
{
    InitBuffer();
    InitDiskDriver(); 
    InitFileSystem();
    InitUser();

    // 初始化root目录
    FileManager &fileMgr = Kernel::Instance().GetFileManager();
    fileMgr.rootDirInode = g_InodeTable.IGet(FileSystem::ROOTINO);
    fileMgr.rootDirInode->i_flag &= (~Inode::ILOCK);
    pthread_mutex_unlock(& fileMgr.rootDirInode->mutex);

    Kernel::Instance().GetFileSystem().LoadSuperBlock();
    User &us = Kernel::Instance().GetUser();
    us.u_cdir = g_InodeTable.IGet(FileSystem::ROOTINO);
    us.u_cdir->i_flag &= (~Inode::ILOCK);
    pthread_mutex_unlock(& us.u_cdir->mutex);
    strcpy(us.u_curdir, "/");

    //初始化文件树
    //现在在’/‘下
    assert(initDir("bin")==0);
    assert(initDir("etc")==0);
    assert(initDir("home")==0);
    assert(initDir("lib")==0);
    assert(initDir("mnt")==0);
    assert(initDir("proc")==0);
    assert(initDir("root")==0);
    assert(initDir("usr")==0);

    printf("文件系统初始化完毕.\n");
}

// 退出
void Kernel::Quit()
{
    this->m_BufferManager->Bflush();
    this->m_FileManager->m_InodeTable->UpdateInodeTable();
    this->m_FileSystem->Update();
    this->m_DiskDriver->quit();
}

int Kernel::initDir(string path)
{
    FileManager &fileMgr = Kernel::Instance().GetFileManager();
    User &u = Kernel::Instance().GetUser();
    u.u_error = NOERROR;
    char tmp[100] = {0};
    strcpy(tmp, path.c_str());
    u.u_dirp = tmp;
    u.u_arg[1] = 040755;
    u.u_arg[2] = 0;
    fileMgr.MkNod();
    return u.u_ar0[User::EAX];
}

// 初始化函数
void Kernel::InitDiskDriver()
{
	this->m_DiskDriver = &g_DiskDriver;

	printf("Initialize Disk Driver...");
	this->GetDiskDriver().Initialize();
	printf("OK.\n");
}
void Kernel::InitBuffer()
{
	this->m_BufferManager = &g_BufferManager;

	printf("Initialize Buffer...");
	this->GetBufferManager().Initialize();
	printf("OK.\n");
}
void Kernel::InitFileSystem()
{
	this->m_FileSystem = &g_FileSystem;
	this->m_FileManager = &g_FileManager;

	printf("Initialize File System...");
	this->GetFileSystem().Initialize();
	printf("OK.\n");

	printf("Initialize File Manager...");
	this->GetFileManager().Initialize();
	printf("OK.\n");
}
void Kernel::InitUser()
{
    this->m_User = &g_User;
}

// 接口函数
Kernel& Kernel::Instance()
{
    return Kernel::instance;
}
DiskDriver &Kernel::GetDiskDriver()
{
    return *(this->m_DiskDriver);
}
BufferManager & Kernel::GetBufferManager()
{
    return *(this->m_BufferManager);
}
FileSystem & Kernel::GetFileSystem()
{
    return *(this->m_FileSystem);
}
FileManager &Kernel::GetFileManager()
{
    return *(this->m_FileManager);
}
User &Kernel::GetUser()
{
    return *(this->m_User);
}