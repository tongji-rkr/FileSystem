#include "DiskDriver.h"
#include "Kernel.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <iostream>
using namespace std;

DiskDriver::DiskDriver()
{
}
DiskDriver::~DiskDriver()
{
}

void DiskDriver::Initialize()//初始化磁盘驱动
{
    this->m_BufferManager = &Kernel::Instance().GetBufferManager();//获取缓冲区管理器

    int fd = open(img_path, O_RDWR);//打开磁盘文件
    if (fd == -1)
    {
        fd = open(img_path, O_RDWR | O_CREAT, 0666);
        if (fd == -1)
        {
            printf("磁盘文件 %s 创建失败\n", img_path);
            exit(-1);
        }
        //对磁盘进行初始化
        this->init_img(fd);
    }
    printf("\n磁盘格式化完成...");
    mmap_img(fd);//将磁盘映射到内存
    this->img_fd = fd;
    printf("\n磁盘 mmap 映射完成...");
}

void DiskDriver::quit()//退出磁盘驱动
{
    struct stat st;
    int r = fstat(this->img_fd, &st);//获取文件信息
    if (r == -1)//获取失败
    {
        printf("获取磁盘文件信息失败，异常退出\n");
        close(this->img_fd);
        exit(-1);
    }
    msync((void *)(this->m_BufferManager->GetP()), st.st_size, MS_SYNC);//将内存中的数据同步到磁盘
}

void DiskDriver::init_img(int fd)
{
    // 初始化SuperBlock
    SuperBlock spb;
    init_super_block(spb);

    // 初始化i节点表
    DiskInode *disk_inode_table = new DiskInode[FileSystem::INODE_ZONE_SIZE * FileSystem::INODE_NUMBER_PER_SECTOR];
    disk_inode_table[0].d_mode = Inode::IFDIR;  // 文件类型为目录文件
    disk_inode_table[0].d_mode |= Inode::IEXEC; // 文件的执行权限

    // 初始化数据块
    char* data_block = new char[FileSystem::DATA_ZONE_SIZE * 512];
    memset(data_block, 0, FileSystem::DATA_ZONE_SIZE * 512);
    init_data_block(data_block);

    // 将数据写入磁盘
    write(fd, &spb, sizeof(SuperBlock));
    write(fd, disk_inode_table, FileSystem::INODE_ZONE_SIZE * FileSystem::INODE_NUMBER_PER_SECTOR * sizeof(DiskInode));
    write(fd, data_block, FileSystem::DATA_ZONE_SIZE * 512);


}

void DiskDriver::init_super_block(SuperBlock& sb)//初始化SuperBlock
{
    sb.s_isize = FileSystem::INODE_ZONE_SIZE;//i节点区大小
    sb.s_fsize = FileSystem::DATA_ZONE_END_SECTOR + 1;//数据区大小
    sb.s_nfree = (FileSystem::DATA_ZONE_SIZE - 99) % 100;//空闲盘块组的个数
    sb.s_fmod  = 0;
    sb.s_ronly = 0;
    sb.s_ninode = 100;
    for(int i = 0; i < sb.s_ninode; ++i)
        sb.s_inode[i] = i;
    // 找到最后一个盘块组的第一个盘块
    int last_block = FileSystem::DATA_ZONE_START_SECTOR;
    while(1)
    {
        if((last_block + 99) < FileSystem::DATA_ZONE_END_SECTOR)
        {
            last_block += 100;
        }
        else
        {
            break;
        }
    }
    last_block--;
    for(int i = 0; i < sb.s_nfree; ++i)
    {
        sb.s_free[i] = last_block + i;
    }
}

void DiskDriver::init_data_block(char* data)
{
    free_table tmp_table;
    int last_block_num = FileSystem::DATA_ZONE_SIZE; //未加入索引的盘块的数量
    // 初始化组长盘块
    for(int i = 0; last_block_num; i++)
    {
        tmp_table.nfree = min(last_block_num,100);
        last_block_num -= tmp_table.nfree;

        for (int j = 0; j < tmp_table.nfree; j++)
        {
            if (i == 0 && j == 0)
                tmp_table.free[j] = 0;
            else
            {
                tmp_table.free[j] = 100 * i + j + FileSystem::DATA_ZONE_START_SECTOR - 1;
            }
        }
        memcpy(&data[99 * 512 + i * 100 * 512], (void *)&tmp_table, sizeof(tmp_table));
    }
}

void DiskDriver::mmap_img(int fd)
{
    struct stat st; //定义文件信息结构体
    /*取得文件大小*/
    int r = fstat(fd, &st);
    if (r == -1)
    {
        printf("获取磁盘信息失败，磁盘初始化终止\n");
        close(fd);
        exit(-1);
    }
    /*把文件映射成虚拟内存地址*/
    char *addr = (char*)mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    this->m_BufferManager->SetP(addr);
}
