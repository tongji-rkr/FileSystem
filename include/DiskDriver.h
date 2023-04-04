#ifndef DISK_DRIVER_H
#define DISK_DRIVER_H
#include "FileSystem.h"
#include "BufferManager.h"

class DiskDriver
{
public:
    DiskDriver();                           /* 构造函数 */
    ~DiskDriver();                          /* 析构函数 */
    void Initialize();                      /* 初始化磁盘驱动器 */
    void quit();                            /* 退出磁盘驱动器 */
private:
    void init_img(int fd);                  /* 初始化磁盘文件 */
    void init_super_block(SuperBlock &sb);  /* 初始化SuperBlock */ 
    void init_data_block(char* data);       /* 初始化数据块 */
    void mmap_img(int fd);                  /* 将磁盘文件映射到内存 */
private:
    const char* img_path = "c.img";         /* 磁盘文件路径 */
    int img_fd;                             /* 磁盘文件描述符 */
    BufferManager *m_BufferManager;         /* 指向缓存管理器的指针 */
};

class free_table
{
public:
    int nfree;                              /* 空闲盘块数 */
    int free[100];                          /* 空闲盘块号数组 */
};

#endif
