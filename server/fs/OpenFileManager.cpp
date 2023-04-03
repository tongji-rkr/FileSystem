/*
 * @Description: this file defines the class OpenFileTable
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */
#include "OpenFileManager.h"
#include "Kernel.h"
#include "TimeInterrupt.h"
#include "Video.h"

/*==============================class OpenFileTable===================================*/
/* 系统全局打开文件表对象实例的定义 */
OpenFileTable g_OpenFileTable;

/*构造函数*/
OpenFileTable::OpenFileTable()
{
	// nothing to do here
}

/*析构函数*/
OpenFileTable::~OpenFileTable()
{
	// nothing to do here
}

/*作用：进程打开文件描述符表中找的空闲项  之 下标  写入 u_ar0[EAX]*/
File *OpenFileTable::FAlloc()
{
	int fd;									/* 用于存放打开文件描述符的值 */
	User &u = Kernel::Instance().GetUser(); /*获得当前用户User结构*/

	/* 在进程打开文件描述符表中获取一个空闲项 */
	fd = u.u_ofiles.AllocFreeSlot();

	if (fd < 0) /* 如果寻找空闲项失败 */
	{
		return NULL;
	}

	/* 在系统全局打开文件表中寻找一个空闲项 */
	for (int i = 0; i < OpenFileTable::NFILE; i++)
	{
		/* f_count==0表示该项空闲 */
		if (this->m_File[i].f_count == 0)
		{
			/* 建立描述符和File结构的勾连关系 */
			u.u_ofiles.SetF(fd, &this->m_File[i]);
			/* 增加对file结构的引用计数 */
			this->m_File[i].f_count++;
			/* 清空文件读、写位置 */
			this->m_File[i].f_offset = 0;
			/*将File结构的指针返回*/
			return (&this->m_File[i]);
		}
	}

	/* 如果没有找到空闲项，显示错误*/
	Diagnose::Write("No Free File Struct\n");
	/*设置错误码为没有找到空闲项*/
	u.u_error = User::ENFILE;
	return NULL;
}

/*对打开文件控制块File结构的引用计数f_count减1，若引用计数f_count为0，则释放File结构。*/
void OpenFileTable::CloseF(File *pFile)
{
	Inode *pNode;													  /* 用于存放文件对应的Inode结构 */
	ProcessManager &procMgr = Kernel::Instance().GetProcessManager(); /*获得进程管理器的引用*/

	/* 管道类型 */
	if (pFile->f_flag & File::FPIPE)
	{
		pNode = pFile->f_inode;							  /* 获得文件对应的Inode结构 */
		pNode->i_mode &= ~(Inode::IREAD | Inode::IWRITE); /* 清除读写标志 */
		procMgr.WakeUpAll((unsigned long)(pNode + 1));	  /* 唤醒所有等待该管道的进程 */
		procMgr.WakeUpAll((unsigned long)(pNode + 2));	  /* 唤醒所有等待该管道的进程 */
	}

	if (pFile->f_count <= 1)
	{
		/*
		 * 如果当前进程是最后一个引用该文件的进程，
		 * 对特殊块设备、字符设备文件调用相应的关闭函数
		 */
		pFile->f_inode->CloseI(pFile->f_flag & File::FWRITE); /* 调用Inode结构的CloseI函数 */
		g_InodeTable.IPut(pFile->f_inode);					  /* 释放Inode结构 */
	}

	/* 引用当前File的进程数减1 */
	pFile->f_count--;
}

/*==============================class InodeTable===================================*/
/*  定义内存Inode表的实例 */
InodeTable g_InodeTable;

/*构造函数*/
InodeTable::InodeTable()
{
	// nothing to do here
}

/*析构函数*/
InodeTable::~InodeTable()
{
	// nothing to do here
}

/*作用：初始化内存Inode表*/
void InodeTable::Initialize()
{
	/* 获取对g_FileSystem的引用 */
	this->m_FileSystem = &Kernel::Instance().GetFileSystem(); /*获得文件系统的引用*/
}

/*作用：根据指定设备dev和Inode编号inumber，从内存Inode表中查找对应的内存Inode结构，如果该Inode已经在内存中，对其上锁并返回该内存Inode，如果不在内存中，则将其读入内存后上锁并返回该内存Inode*/
Inode *InodeTable::IGet(short dev, int inumber)
{
	Inode *pInode;							/* 用于存放内存Inode结构的指针 */
	User &u = Kernel::Instance().GetUser(); /*获得当前用户User结构*/

	while (true)
	{
		/* 检查指定设备dev中编号为inumber的外存Inode是否有内存拷贝 */
		int index = this->IsLoaded(dev, inumber);
		if (index >= 0) /* 找到内存拷贝 */
		{
			/* 获得内存Inode结构的指针 */
			pInode = &(this->m_Inode[index]);
			/* 如果该内存Inode被上锁 */
			if (pInode->i_flag & Inode::ILOCK)
			{
				/* 增设IWANT标志，然后睡眠 */
				pInode->i_flag |= Inode::IWANT;
				/* 睡眠，等待该内存Inode解锁 */
				u.u_procp->Sleep((unsigned long)&pInode, ProcessManager::PINOD);

				/* 回到while循环，需要重新搜索，因为该内存Inode可能已经失效 */
				continue;
			}

			/* 如果该内存Inode用于连接子文件系统，查找该Inode对应的Mount装配块 */
			if (pInode->i_flag & Inode::IMOUNT)
			{
				/* 获得该Inode对应的Mount装配块 */
				Mount *pMount = this->m_FileSystem->GetMount(pInode);
				if (NULL == pMount)
				{
					/* 没有找到 */
					Utility::Panic("No Mount Tab...");
				}
				else
				{
					/* 将参数设为子文件系统设备号、根目录Inode编号 */
					dev = pMount->m_dev;
					/* 将参数设为子文件系统根目录Inode编号 */
					inumber = FileSystem::ROOTINO;
					/* 回到while循环，以新dev，inumber值重新搜索 */
					continue;
				}
			}

			/*
			 * 程序执行到这里表示：内存Inode高速缓存中找到相应内存Inode，
			 * 增加其引用计数，增设ILOCK标志并返回之
			 */
			pInode->i_count++;				/* 增加引用计数 */
			pInode->i_flag |= Inode::ILOCK; /* 增设ILOCK标志 */
			return pInode;					/* 返回内存Inode结构的指针 */
		}
		else /* 没有Inode的内存拷贝，则分配一个空闲内存Inode */
		{
			pInode = this->GetFreeInode();
			/* 若内存Inode表已满，分配空闲Inode失败 */
			if (NULL == pInode)
			{
				/* 输出错误信息，设置错误号，返回NULL */
				Diagnose::Write("Inode Table Overflow !\n"); /* 输出错误信息 */
				u.u_error = User::ENFILE;					 /* 设置错误号为没有可用Inode */
				return NULL;								 /* 返回NULL */
			}
			else /* 分配空闲Inode成功，将外存Inode读入新分配的内存Inode */
			{
				/* 设置新的设备号、外存Inode编号，增加引用计数，对索引节点上锁 */
				pInode->i_dev = dev;		   /* 设置设备号 */
				pInode->i_number = inumber;	   /* 设置外存Inode编号 */
				pInode->i_flag = Inode::ILOCK; /* 增设ILOCK标志 */
				pInode->i_count++;			   /* 增加引用计数 */
				pInode->i_lastr = -1;		   /* 初始化最后一次读取的逻辑块号为-1 */

				BufferManager &bm = Kernel::Instance().GetBufferManager();
				/* 将该外存Inode读入缓冲区 */
				Buf *pBuf = bm.Bread(dev, FileSystem::INODE_ZONE_START_SECTOR + inumber / FileSystem::INODE_NUMBER_PER_SECTOR); /* 读入外存Inode */

				/* 如果发生I/O错误 */
				if (pBuf->b_flags & Buf::B_ERROR)
				{
					/* 释放缓存 */
					bm.Brelse(pBuf);
					/* 释放占据的内存Inode */
					this->IPut(pInode);
					return NULL;
				}

				/* 将缓冲区中的外存Inode信息拷贝到新分配的内存Inode中 */
				pInode->ICopy(pBuf, inumber);
				/* 释放缓存 */
				bm.Brelse(pBuf);
				return pInode;
			}
		}
	}
	return NULL; /* GCC likes it! */
}

/* close文件时会调用Iput
 *      主要做的操作：内存i节点计数 i_count--；若为0，释放内存 i节点、若有改动写回磁盘
 * 搜索文件途径的所有目录文件，搜索经过后都会Iput其内存i节点。路径名的倒数第2个路径分量一定是个
 *   目录文件，如果是在其中创建新文件、或是删除一个已有文件；再如果是在其中创建删除子目录。那么
 *   	必须将这个目录文件所对应的内存 i节点写回磁盘。
 *   	这个目录文件无论是否经历过更改，我们必须将它的i节点写回磁盘。
 * */
void InodeTable::IPut(Inode *pNode)
{
	/* 当前进程为引用该内存Inode的唯一进程，且准备释放该内存Inode */
	if (pNode->i_count == 1)
	{
		/*
		 * 上锁，因为在整个释放过程中可能因为磁盘操作而使得该进程睡眠，
		 * 此时有可能另一个进程会对该内存Inode进行操作，这将有可能导致错误。
		 */
		pNode->i_flag |= Inode::ILOCK;

		/* 该文件已经没有目录路径指向它 */
		if (pNode->i_nlink <= 0)
		{
			/* 释放该文件占据的数据盘块 */
			pNode->ITrunc();
			pNode->i_mode = 0; /* 设置文件类型为无效类型 */
			/* 释放对应的外存Inode */
			this->m_FileSystem->IFree(pNode->i_dev, pNode->i_number);
		}

		/* 更新外存Inode信息 */
		pNode->IUpdate(Time::time);

		/* 解锁内存Inode，并且唤醒等待进程 */
		pNode->Prele();
		/* 清除内存Inode的所有标志位 */
		pNode->i_flag = 0;
		/* 这是内存inode空闲的标志之一，另一个是i_count == 0 */
		pNode->i_number = -1;
	}

	/* 减少内存Inode的引用计数，唤醒等待进程 */
	pNode->i_count--;
	pNode->Prele();
}

/*将所有被修改过的内存Inode更新到对应外存Inode中*/
void InodeTable::UpdateInodeTable()
{
	/* 遍历内存Inode表 */
	for (int i = 0; i < InodeTable::NINODE; i++)
	{
		/*
		 * 如果Inode对象没有被上锁，即当前未被其它进程使用，可以同步到外存Inode；
		 * 并且count不等于0，count == 0意味着该内存Inode未被任何打开文件引用，无需同步。
		 */
		if ((this->m_Inode[i].i_flag & Inode::ILOCK) == 0 && this->m_Inode[i].i_count != 0)
		{
			/* 将内存Inode上锁后同步到外存Inode */
			this->m_Inode[i].i_flag |= Inode::ILOCK;
			this->m_Inode[i].IUpdate(Time::time);

			/* 对内存Inode解锁 */
			this->m_Inode[i].Prele();
		}
	}
}

/* 检查设备dev上编号为inumber的外存inode是否有内存拷贝，如果有则返回该内存Inode在内存Inode表中的索引*/
int InodeTable::IsLoaded(short dev, int inumber)
{
	/* 寻找指定外存Inode的内存拷贝 */
	for (int i = 0; i < InodeTable::NINODE; i++)
	{
		if (this->m_Inode[i].i_dev == dev && this->m_Inode[i].i_number == inumber && this->m_Inode[i].i_count != 0)
		{
			/* 找到，返回内存Inode在内存Inode表中的索引 */
			return i;
		}
	}
	return -1;
}

/* 从内存Inode表中获取一个空闲的内存Inode */
Inode *InodeTable::GetFreeInode()
{
	for (int i = 0; i < InodeTable::NINODE; i++)
	{
		/* 如果该内存Inode引用计数为零，则该Inode表示空闲 */
		if (this->m_Inode[i].i_count == 0)
		{
			/* 返回该空闲的内存Inode */
			return &(this->m_Inode[i]);
		}
	}
	return NULL; /* 寻找失败 */
}
