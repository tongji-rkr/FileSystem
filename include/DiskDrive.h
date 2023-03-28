// 负责使用LinuxAPI对模拟磁盘文件进行操作（包括磁盘的初始化）
#ifndef DISK_DRIVE_H
#define DISK_DRIVE_H
#include "FileSystem.h"
#include "BufferManager.h"

class DiskDrive
{
public:
    DiskDrive();
    ~DiskDrive();
    void Initialize();
    void quit();
private:
    void init_spb(SuperBlock &sb);
    void init_db(char* data);
    void init_img(int fd);
    void mmap_img(int fd);
private:
    // 磁盘缓存文件地址
    const char* devpath = "myDisk.img";
    // 磁盘缓存文件fd
    int img_fd; 
    // FileSystem类需要缓存管理模块(BufferManager)提供的接口
    BufferManager *m_BufferManager; 
};

#endif
