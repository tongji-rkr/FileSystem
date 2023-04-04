#ifndef KERNEL_H
#define KERNEL_H

#include "DiskDriver.h"
#include "FileManager.h"
#include "FileSystem.h"
#include "BufferManager.h"
#include "User.h"
#include <string>

class Kernel
{
public:
    Kernel();                           /* 构造函数 */
    ~Kernel();                          /* 析构函数 */
    void Initialize();                  /* 初始化内核 */
    void Quit();                        /* 退出内核 */
    //接口函数
    static Kernel& Instance();          /* 获取单体实例 */
    DiskDriver& GetDiskDriver();        /* 获取磁盘驱动器 */
    BufferManager& GetBufferManager();  /* 获取缓存管理器 */
    FileSystem& GetFileSystem();        /* 获取文件系统 */
    FileManager& GetFileManager();      /* 获取文件管理器 */
    User& GetUser();                    /* 获取用户 */
private:
    void InitDiskDriver();              /* 初始化磁盘驱动器 */
    void InitFileSystem();              /* 初始化文件系统 */
    void InitBuffer();                  /* 初始化缓存管理器 */
    void InitUser();                    /* 初始化用户 */
    int initDir(string path);           /* 初始化目录 */

private:
    static Kernel instance;             /* 单体实例 */   
    DiskDriver* m_DiskDriver;           /* 磁盘驱动器 */
    BufferManager*  m_BufferManager;    /* 缓存管理器 */
    FileSystem*  m_FileSystem;          /* 文件系统 */
    FileManager* m_FileManager;         /* 文件管理器 */
    User* m_User;                       /* 用户 */
};

#endif

