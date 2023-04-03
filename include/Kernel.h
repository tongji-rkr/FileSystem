/*
 * @Description: 
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-03-22 21:00:31
 */
#ifndef KERNEL_H
#define KERNEL_H

#include "PageManager.h"
#include "ProcessManager.h"
#include "KernelAllocator.h"
#include "User.h"
#include "BufferManager.h"
#include "DeviceManager.h"
#include "FileManager.h"
#include "FileSystem.h"
#include "SwapperManager.h"

/*
 * Kernel类用于封装所有内核相关的全局类实例对象，
 * 例如PageManager, ProcessManager等。
 * 
 * Kernel类在内存中为单体模式，保证内核中封装各内核
 * 模块的对象都只有一个副本。
 */
class Kernel
{
public:
	static const unsigned long USER_ADDRESS = 0x400000 - 0x1000 + 0xc0000000;	/* 0xC03FF000  user结构的起始虚地址*/
	static const unsigned long USER_PAGE_INDEX = 1023;		/* USER_ADDRESS对应页表项在PageTable中的索引 */

public:
	Kernel();	//构造函数
	~Kernel();	//析构函数
	static Kernel& Instance();	//返回Kernel实例
	void Initialize();		/* 该函数完成初始化内核大部分数据结构的初始化 */

	KernelPageManager& GetKernelPageManager();	//返回m_KernelPageManager
	UserPageManager& GetUserPageManager();	//返回m_UserPageManager
	ProcessManager& GetProcessManager();	//返回m_ProcessManager
	KernelAllocator& GetKernelAllocator();	//返回m_ProcessManager
	SwapperManager& GetSwapperManager();	//返回m_SwapperManager
	BufferManager& GetBufferManager();	//返回m_BufferManager
	DeviceManager& GetDeviceManager();	//返回m_DeviceManager
	FileSystem& GetFileSystem();	//返回m_FileSystem
	FileManager& GetFileManager();	//返回m_FileManager
	User& GetUser();		/* 获取当前进程的User结构 */

private:
	void InitMemory();	//内存初始化
	void InitProcess();	//进程初始化
	void InitBuffer();	//buffer初始化
	void InitFileSystem();	//文件系统初始化

private:
	static Kernel instance;		/* Kernel单体类实例 */

	KernelPageManager* m_KernelPageManager;	//内核页表管理
	UserPageManager* m_UserPageManager;	//用户页表管理
	ProcessManager* m_ProcessManager;	//进程管理
	KernelAllocator* m_KernelAllocator;	//内核内存分类
	SwapperManager* m_SwapperManager;	//交换区管理
	BufferManager* m_BufferManager;	//缓冲区管理
	DeviceManager* m_DeviceManager;	//设备管理
	FileSystem* m_FileSystem;	//文件系统
	FileManager* m_FileManager;	//文件管理
};

#endif
