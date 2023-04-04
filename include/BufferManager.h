#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include "Buf.h"

class BufferManager
{
public:
    /* static const member */
    static const int NBUF = 15;         /* 缓存控制块、缓冲区的数量 */
    static const int BUFFER_SIZE = 512; /* 缓冲区大小。 以字节为单位 */

public:
    BufferManager();            /* 构造函数 */
    ~BufferManager();           /* 析构函数 */

    void Initialize();          /* 初始化缓存管理器 */
    Buf *GetBlk(int blkno);     /* 从缓存中取得指定字符块 */
    void Brelse(Buf *bp);       /* 释放缓存中的指定字符块 */

    //读写
    Buf *Bread(int blkno);      /* 读指定字符块 */
    void Bwrite(Buf *bp);       /* 写指定字符块 */                            
    void Bdwrite(Buf *bp);      /* 延迟写指定字符块 */       
    void Bawrite(Buf *bp);      /* 异步写指定字符块 */   

    void ClrBuf(Buf *bp);       /* 清除指定缓存控制块的标志位 */
    void Bflush();              /* 将缓存中的所有修改过的字符块写回磁盘 */
    Buf &GetBFreeList();        /* 获取自由缓存队列控制块 */

    void SetP(char* mmapaddr);  /* 设置mmap映射到内存后的起始地址 */
    char* GetP();               /* 获取mmap映射到内存后的起始地址 */

private:
    void GetError(Buf *bp);     /* 获取I/O操作中发生的错误信息 */
    void NotAvail(Buf *bp);     /* 从自由队列中摘下指定的缓存控制块buf */
    Buf *InCore(int blkno);     /* 检查指定字符块是否已在缓存中 */

private:
    Buf bFreeList;                           /* 自由缓存队列控制块 */
    Buf m_Buf[NBUF];                         /* 缓存控制块数组 */
    unsigned char Buffer[NBUF][BUFFER_SIZE]; /* 缓冲区数组 */
	char *p;                                 /* mmap映射到内存后的起始地址 */
};

#endif