/*
 * @Description:
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:41:30
 */
#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include "Buf.h"
#include "Utility.h"

/* 块设备表devtab定义 */
class Devtab
{
public:
	Devtab();  // 构造函数
	~Devtab(); // 析构函数

public:
	int d_active; // 磁盘正在执行IO操作时，d_active是1
	int d_errcnt; // 当前扇区IO错误计数器，出现错误时，该变量计数到10时系统才会放弃
	Buf *b_forw;  // 设备缓存队列的队首
	Buf *b_back;  // 设备缓存队列的队尾
	Buf *d_actf;  // IO 请求队列的队首
	Buf *d_actl;  // IO 请求队列的队尾
};

/*
 * 块设备基类，各类块设备都从此基类继承。
 * 派生类override基类中的Open(), Close(), Strategy()函数，
 * 实现对各中块设备不同的操作逻辑。以此替代Unix V6中
 * 原块设备开关表(bdevsw)的功能。
 */
class BlockDevice
{
public:
	BlockDevice();			  // 构造函数
	BlockDevice(Devtab *tab); // 使用块设备表tab初始化
	virtual ~BlockDevice();	  // 析构函数
	/*
	 * 定义为虚函数，由派生类进行override实现设备
	 * 特定操作。正常情况下，基类中函数不应被调用到。
	 */
	virtual int Open(short dev, int mode);	// 报错
	virtual int Close(short dev, int mode); // 报错
	virtual int Strategy(Buf *bp);			// 报错
	virtual void Start();					// 报错

public:
	Devtab *d_tab; /* 指向块设备表的指针 */
};

/* ATA磁盘设备派生类。从块设备基类BlockDevice继承而来。 */
class ATABlockDevice : public BlockDevice
{
public:
	static int NSECTOR; /* ATA磁盘扇区数 */

public:
	ATABlockDevice(Devtab *tab);
	virtual ~ATABlockDevice();
	/*
	 * Override基类BlockDevice中的虚函数，实现
	 * 派生类ATABlockDevice特定的设备操作逻辑。
	 */
	int Open(short dev, int mode);	// 为空，等速圆周运转的硬盘不需要打开关闭
	int Close(short dev, int mode); // 为空，等速圆周运转的硬盘不需要打开关闭
	int Strategy(Buf *bp);			// 维护 FIFS 的请求队列。新请求放队尾，函数返回；队列为空，调用Start 函数启动磁盘。
	/*
	 * 启动块设备执行I/O请求。此函数将请求块送入
	 * I/O请求队列，如果硬盘空闲则立即进行操作。
	 */
	void Start();
};

#endif
