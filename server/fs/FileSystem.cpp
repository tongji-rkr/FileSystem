/*
 * @Description: this file defines the class SuperBlock
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */
#include "FileSystem.h"
#include "Utility.h"
#include "New.h"
#include "Kernel.h"
#include "OpenFileManager.h"
#include "TimeInterrupt.h"
#include "Video.h"

/*==============================class SuperBlock===================================*/
/* 系统全局超级块SuperBlock对象 */
SuperBlock g_spb;

SuperBlock::SuperBlock()
{
	// nothing to do here
}

SuperBlock::~SuperBlock()
{
	// nothing to do here
}

/*==============================class Mount===================================*/
/*
 * 文件系统装配块(Mount)的定义。
 * 装配块用于实现子文件系统与
 * 根文件系统的连接。
 */
/*
	块设备的文件系统通过挂在操作被系统识别
*/
Mount::Mount()
{
	this->m_dev = -1;	   /* 文件系统设备号初始化 */
	this->m_spb = NULL;	   /* 指向文件系统的Super Block对象在内存中的副本初始化 */
	this->m_inodep = NULL; /* 指向挂载子文件系统的内存INode初始化 */
}

Mount::~Mount()
{
	this->m_dev = -1;	   /*重置文件系统设备号*/
	this->m_inodep = NULL; /*重置指向挂载子文件系统的内存INode指针*/
	// 释放内存SuperBlock副本
	if (this->m_spb != NULL)
	{
		/*当前装配块中的SuperBlock副本指针不为空，释放内存*/
		delete this->m_spb;
		this->m_spb = NULL;
	}
}

/*==============================class FileSystem===================================*/
/*
 * 文件系统类(FileSystem)管理文件存储设备中
 * 的各类存储资源，磁盘块、外存INode的分配、
 * 释放。
 */
FileSystem::FileSystem()
{
	// nothing to do here
}

FileSystem::~FileSystem()
{
	// nothing to do here
}

/*
 * @comment 初始化成员变量
 */
void FileSystem::Initialize()
{
	this->m_BufferManager = &Kernel::Instance().GetBufferManager(); /* 获得缓冲管理器对象的指针 */
	/* Update()函数的锁，该函数用于同步内存各个SuperBlock副本以及
	 * 存储设备上的SuperBlock副本，防止多个进程同时调用该函数
	 */
	this->updlock = 0;
}

/*该函数用于在系统初始化时读入superblock，同时在mount数组中初始化根文件系统*/
void FileSystem::LoadSuperBlock()
{
	/*获取当前进程的User对象*/
	User &u = Kernel::Instance().GetUser();
	/*获取缓冲管理器对象的引用*/
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager();
	/*定义缓冲区指针*/
	Buf *pBuf;

	/* 读取磁盘上的SuperBlock并在内存中创建副本*/
	for (int i = 0; i < 2; i++)
	{
		/*由于SuperBlock大小1024Bit占用2个盘块，因此需要读取两次*/
		int *p = (int *)&g_spb + i * 128;

		/*通过缓冲管理器读取磁盘上的SuperBlock*/
		pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, FileSystem::SUPER_BLOCK_SECTOR_NUMBER + i);

		/*将读取到的SuperBlock拷贝到内存中的SuperBlock副本中*/
		Utility::DWordCopy((int *)pBuf->b_addr, p, 128);

		/*释放缓冲区*/
		bufMgr.Brelse(pBuf);
	}
	if (User::NOERROR != u.u_error)
	{
		/*读取SuperBlock失败，系统停止运行*/
		Utility::Panic("Load SuperBlock Error....!\n");
	}

	/*初始化根文件系统，填入根文件系统的设备号和SuperBlock副本的地址*/
	this->m_Mount[0].m_dev = DeviceManager::ROOTDEV;
	this->m_Mount[0].m_spb = &g_spb;

	/*更新SuperBlock的相关信息*/
	g_spb.s_flock = 0; /* 文件系统锁 */
	g_spb.s_ilock = 0;
	g_spb.s_ronly = 0;
	g_spb.s_time = Time::time; /* 文件系统最后一次被修改的时间 */
}

/*该函数用于根据文件存储设备的设备号dev获取该文件系统的SuperBlock*/
SuperBlock *FileSystem::GetFS(short dev)
{
	SuperBlock *sb; /* 定义SuperBlock指针 */

	/* 遍历系统装配块表，寻找设备号为dev的设备中文件系统的SuperBlock */
	for (int i = 0; i < FileSystem::NMOUNT; i++)
	{
		if (this->m_Mount[i].m_spb != NULL && this->m_Mount[i].m_dev == dev)
		{
			/* 获取SuperBlock的地址 */
			sb = this->m_Mount[i].m_spb;
			if (sb->s_nfree > 100 || sb->s_ninode > 100)
			{
				/* 如果SuperBlock中的空闲盘块数或空闲外存INode数大于100，
				 * 则将其置为0，防止系统崩溃
				 */
				sb->s_nfree = 0;
				sb->s_ninode = 0;
			}
			/* 返回SuperBlock的地址 */
			return sb;
		}
	}

	Utility::Panic("No File System!"); /* 没有找到对应的文件系统，系统停止运行 */
	return NULL;					   /* 为了避免编译器警告，这里返回NULL */
}

/* 该函数用于将SuperBlock对象的内存副本更新到存储设备的SuperBlock中去*/
void FileSystem::Update()
{
	int i;
	SuperBlock *sb;
	Buf *pBuf;

	/* 另一进程正在进行同步，则直接返回 */
	if (this->updlock)
	{
		return;
	}

	/* 设置Update()函数的互斥锁，防止其它进程重入 */
	this->updlock++;

	/* 同步SuperBlock到磁盘 */
	for (i = 0; i < FileSystem::NMOUNT; i++)
	{
		/* 如果该Mount装配块对应某个文件系统 */
		if (this->m_Mount[i].m_spb != NULL) /* 该Mount装配块对应某个文件系统 */
		{
			sb = this->m_Mount[i].m_spb; /* 获取SuperBlock的地址 */

			/* 如果该SuperBlock内存副本没有被修改，直接管理inode和空闲盘块被上锁或该文件系统是只读文件系统 */
			if (sb->s_fmod == 0 || sb->s_ilock != 0 || sb->s_flock != 0 || sb->s_ronly != 0)
			{
				continue;
			}

			/* 清SuperBlock修改标志 */
			sb->s_fmod = 0;
			/* 写入SuperBlock最后存访时间 */
			sb->s_time = Time::time;

			/*
			 * 为将要写回到磁盘上去的SuperBlock申请一块缓存，由于缓存块大小为512字节，
			 * SuperBlock大小为1024字节，占据2个连续的扇区，所以需要2次写入操作。
			 */
			for (int j = 0; j < 2; j++)
			{
				/* 第一次p指向SuperBlock的第0字节，第二次p指向第512字节 */
				int *p = (int *)sb + j * 128;

				/* 将要写入到设备dev上的SUPER_BLOCK_SECTOR_NUMBER + j扇区中去 */
				pBuf = this->m_BufferManager->GetBlk(this->m_Mount[i].m_dev, FileSystem::SUPER_BLOCK_SECTOR_NUMBER + j);

				/* 将SuperBlock中第0 - 511字节写入缓存区 */
				Utility::DWordCopy(p, (int *)pBuf->b_addr, 128);

				/* 将缓冲区中的数据写到磁盘上 */
				this->m_BufferManager->Bwrite(pBuf);
			}
		}
	}

	/* 同步修改过的内存Inode到对应外存Inode */
	g_InodeTable.UpdateInodeTable();

	/* 清除Update()函数锁 */
	this->updlock = 0;

	/* 将延迟写的缓存块写到磁盘上 */
	this->m_BufferManager->Bflush(DeviceManager::NODEV);
}

/* 该函数用于从设备dev上分配一个空闲的外存Inode */
Inode *FileSystem::IAlloc(short dev)
{
	SuperBlock *sb;							/* 定义SuperBlock指针 */
	Buf *pBuf;								/* 定义缓冲区指针 */
	Inode *pNode;							/* 定义Inode指针 */
	User &u = Kernel::Instance().GetUser(); /* 获取当前进程的User结构 */
	int ino;								/* 分配到的空闲外存Inode编号 */

	/* 获取相应设备的SuperBlock内存副本 */
	sb = this->GetFS(dev);

	/* 如果SuperBlock空闲Inode表被上锁，则睡眠等待至解锁 */
	while (sb->s_ilock)
	{
		/* 睡眠等待至SuperBlock空闲Inode表解锁 */
		u.u_procp->Sleep((unsigned long)&sb->s_ilock, ProcessManager::PINOD);
	}

	/*
	 * SuperBlock直接管理的空闲Inode索引表已空，
	 * 必须到磁盘上搜索空闲Inode。先对inode列表上锁，
	 * 因为在以下程序中会进行读盘操作可能会导致进程切换，
	 * 其他进程有可能访问该索引表，将会导致不一致性。
	 */
	if (sb->s_ninode <= 0)
	{
		/* 空闲Inode索引表上锁 */
		sb->s_ilock++;

		/* 外存Inode编号从0开始，这不同于Unix V6中外存Inode从1开始编号 */
		ino = -1;

		/* 依次读入磁盘Inode区中的磁盘块，搜索其中空闲外存Inode，记入空闲Inode索引表 */
		for (int i = 0; i < sb->s_isize; i++)
		{
			/* 读入磁盘Inode区中的第i个磁盘块 */
			pBuf = this->m_BufferManager->Bread(dev, FileSystem::INODE_ZONE_START_SECTOR + i);

			/* 获取缓冲区首址 */
			int *p = (int *)pBuf->b_addr;

			/* 检查该缓冲区中每个外存Inode的i_mode != 0，表示已经被占用 */
			for (int j = 0; j < FileSystem::INODE_NUMBER_PER_SECTOR; j++)
			{
				/* 外存Inode编号加1 */
				ino++;

				/* 获取外存Inode的i_mode */
				int mode = *(p + j * sizeof(DiskInode) / sizeof(int));

				/* 该外存Inode已被占用，不能记入空闲Inode索引表 */
				if (mode != 0)
				{
					continue;
				}

				/*
				 * 如果外存inode的i_mode==0，此时并不能确定
				 * 该inode是空闲的，因为有可能是内存inode没有写到
				 * 磁盘上,所以要继续搜索内存inode中是否有相应的项
				 */
				if (g_InodeTable.IsLoaded(dev, ino) == -1)
				{
					/* 该外存Inode没有对应的内存拷贝，将其记入空闲Inode索引表 */
					sb->s_inode[sb->s_ninode++] = ino;

					/* 如果空闲索引表已经装满，则不继续搜索 */
					if (sb->s_ninode >= 100)
					{
						break;
					}
				}
			}

			/* 至此已读完当前磁盘块，释放相应的缓存 */
			this->m_BufferManager->Brelse(pBuf);

			/* 如果空闲索引表已经装满，则不继续搜索 */
			if (sb->s_ninode >= 100)
			{
				break;
			}
		}
		/* 解除对空闲外存Inode索引表的锁，唤醒因为等待锁而睡眠的进程 */
		sb->s_ilock = 0;
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)&sb->s_ilock);

		/* 如果在磁盘上没有搜索到任何可用外存Inode，返回NULL */
		if (sb->s_ninode <= 0)
		{
			Diagnose::Write("No Space On %d !\n", dev);
			u.u_error = User::ENOSPC; /* 设置错误号 */
			return NULL;
		}
	}

	/*
	 * 上面部分已经保证，除非系统中没有可用外存Inode，
	 * 否则空闲Inode索引表中必定会记录可用外存Inode的编号。
	 */
	while (true)
	{
		/* 从索引表“栈顶”获取空闲外存Inode编号 */
		ino = sb->s_inode[--sb->s_ninode];

		/* 将空闲Inode读入内存 */
		pNode = g_InodeTable.IGet(dev, ino);
		/* 未能分配到内存inode */
		if (NULL == pNode)
		{
			return NULL;
		}

		/* 如果该Inode空闲,清空Inode中的数据 */
		if (0 == pNode->i_mode)
		{
			pNode->Clean();
			/* 设置SuperBlock被修改标志 */
			sb->s_fmod = 1;
			return pNode;
		}
		else /* 如果该Inode已被占用 */
		{
			g_InodeTable.IPut(pNode);
			continue; /* while循环 */
		}
	}
	return NULL; /* GCC likes it! */
}

/* 本函数用以释放外存Inode */
void FileSystem::IFree(short dev, int number)
{
	SuperBlock *sb; /* 超级块内存副本 */

	sb = this->GetFS(dev); /* 获取相应设备的SuperBlock内存副本 */

	/*
	 * 如果超级块直接管理的空闲Inode表上锁，
	 * 则释放的外存Inode散落在磁盘Inode区中。
	 */
	if (sb->s_ilock)
	{
		/* 设置SuperBlock被修改标志 */
		return;
	}

	/*
	 * 如果超级块直接管理的空闲外存Inode超过100个，
	 * 同样让释放的外存Inode散落在磁盘Inode区中。
	 */
	if (sb->s_ninode >= 100)
	{
		/* 设置SuperBlock被修改标志 */
		return;
	}

	/* 将释放的外存Inode编号记录到空闲Inode索引表中 */
	sb->s_inode[sb->s_ninode++] = number;

	/* 设置SuperBlock被修改标志 */
	sb->s_fmod = 1;
}

/* 本函数用以在存储设备dev上分配空闲磁盘块 */
Buf *FileSystem::Alloc(short dev)
{
	int blkno;								/* 分配到的空闲磁盘块编号 */
	SuperBlock *sb;							/* 超级块内存副本 */
	Buf *pBuf;								/* 缓冲区指针 */
	User &u = Kernel::Instance().GetUser(); /* 获取当前进程的User结构 */

	/* 获取SuperBlock对象的内存副本 */
	sb = this->GetFS(dev);

	/*
	 * 如果空闲磁盘块索引表正在被上锁，表明有其它进程
	 * 正在操作空闲磁盘块索引表，因而对其上锁。这通常
	 * 是由于其余进程调用Free()或Alloc()造成的。
	 */
	while (sb->s_flock)
	{
		/* 进入睡眠直到获得该锁才继续 */
		u.u_procp->Sleep((unsigned long)&sb->s_flock, ProcessManager::PINOD);
	}

	/* 从索引表“栈顶”获取空闲磁盘块编号 */
	blkno = sb->s_free[--sb->s_nfree];

	/*
	 * 若获取磁盘块编号为零，则表示已分配尽所有的空闲磁盘块。
	 * 或者分配到的空闲磁盘块编号不属于数据盘块区域中(由BadBlock()检查)，
	 * 都意味着分配空闲磁盘块操作失败。
	 */
	if (0 == blkno)
	{
		sb->s_nfree = 0;							/* 空闲磁盘块索引表中没有空闲磁盘块编号 */
		Diagnose::Write("No Space On %d !\n", dev); /* 打印错误信息 */
		u.u_error = User::ENOSPC;					/* 设置错误号 */
		return NULL;								/* 返回NULL */
	}
	if (this->BadBlock(sb, dev, blkno))
	{
		/* 设置SuperBlock被修改标志 */
		return NULL;
	}

	/*
	 * 栈已空，新分配到空闲磁盘块中记录了下一组空闲磁盘块的编号,
	 * 将下一组空闲磁盘块的编号读入SuperBlock的空闲磁盘块索引表s_free[100]中。
	 */
	if (sb->s_nfree <= 0)
	{
		/*
		 * 此处加锁，因为以下要进行读盘操作，有可能发生进程切换，
		 * 新上台的进程可能对SuperBlock的空闲盘块索引表访问，会导致不一致性。
		 */
		sb->s_flock++;

		/* 读入该空闲磁盘块 */
		pBuf = this->m_BufferManager->Bread(dev, blkno);

		/* 从该磁盘块的0字节开始记录，共占据4(s_nfree)+400(s_free[100])个字节 */
		int *p = (int *)pBuf->b_addr;

		/* 首先读出空闲盘块数s_nfree */
		sb->s_nfree = *p++;

		/* 读取缓存中后续位置的数据，写入到SuperBlock空闲盘块索引表s_free[100]中 */
		Utility::DWordCopy(p, sb->s_free, 100);

		/* 缓存使用完毕，释放以便被其它进程使用 */
		this->m_BufferManager->Brelse(pBuf);

		/* 解除对空闲磁盘块索引表的锁，唤醒因为等待锁而睡眠的进程 */
		sb->s_flock = 0;
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)&sb->s_flock);
	}

	/* 普通情况下成功分配到一空闲磁盘块 */
	pBuf = this->m_BufferManager->GetBlk(dev, blkno); /* 为该磁盘块申请缓存 */
	this->m_BufferManager->ClrBuf(pBuf);			  /* 清空缓存中的数据 */
	sb->s_fmod = 1;									  /* 设置SuperBlock被修改标志 */

	return pBuf;
}

/* 本函数用以释放存储设备dev上的磁盘块blkno */
void FileSystem::Free(short dev, int blkno)
{
	SuperBlock *sb;							/* 超级块内存副本 */
	Buf *pBuf;								/* 缓冲区指针 */
	User &u = Kernel::Instance().GetUser(); /* 获取当前进程的User结构 */

	sb = this->GetFS(dev); /* 获取SuperBlock对象的内存副本 */

	/*
	 * 尽早设置SuperBlock被修改标志，以防止在释放
	 * 磁盘块Free()执行过程中，对SuperBlock内存副本
	 * 的修改仅进行了一半，就更新到磁盘SuperBlock去
	 */
	sb->s_fmod = 1;

	/* 如果空闲磁盘块索引表被上锁，则睡眠等待解锁 */
	while (sb->s_flock)
	{
		/* 进入睡眠直到获得该锁才继续 */
		u.u_procp->Sleep((unsigned long)&sb->s_flock, ProcessManager::PINOD);
	}

	/* 检查释放磁盘块的合法性 */
	if (this->BadBlock(sb, dev, blkno))
	{
		return;
	}

	/*
	 * 如果先前系统中已经没有空闲盘块，
	 * 现在释放的是系统中第1块空闲盘块
	 */
	if (sb->s_nfree <= 0)
	{
		sb->s_nfree = 1;
		sb->s_free[0] = 0; /* 使用0标记空闲盘块链结束标志 */
	}

	/* SuperBlock中直接管理空闲磁盘块号的栈已满 */
	if (sb->s_nfree >= 100)
	{
		/*
		 * 此处加锁，因为以下要进行写盘操作，有可能发生进程切换，
		 * 新上台的进程可能对SuperBlock的空闲盘块索引表访问，会导致不一致性。
		 */
		sb->s_flock++;

		/*
		 * 使用当前Free()函数正要释放的磁盘块，存放前一组100个空闲
		 * 磁盘块的索引表
		 */
		pBuf = this->m_BufferManager->GetBlk(dev, blkno); /* 为当前正要释放的磁盘块分配缓存 */

		/* 从该磁盘块的0字节开始记录，共占据4(s_nfree)+400(s_free[100])个字节 */
		int *p = (int *)pBuf->b_addr;

		/* 首先写入空闲盘块数，除了第一组为99块，后续每组都是100块 */
		*p++ = sb->s_nfree;
		/* 将SuperBlock的空闲盘块索引表s_free[100]写入缓存中后续位置 */
		Utility::DWordCopy(sb->s_free, p, 100);

		sb->s_nfree = 0;
		/* 将存放空闲盘块索引表的“当前释放盘块”写入磁盘，即实现了空闲盘块记录空闲盘块号的目标 */
		this->m_BufferManager->Bwrite(pBuf);
		/* 解除对空闲磁盘块索引表的锁*/
		sb->s_flock = 0;
		/* 唤醒因为等待锁而睡眠的进程 */
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)&sb->s_flock);
	}
	sb->s_free[sb->s_nfree++] = blkno; /* SuperBlock中记录下当前释放盘块号 */
	sb->s_fmod = 1;					   /* 设置SuperBlock被修改标志 */
}

/*本函数查找文件系统装配表，搜索指定Inode对应的Mount装配块*/
Mount *FileSystem::GetMount(Inode *pInode)
{
	/* 遍历系统的装配块表 */
	for (int i = 0; i <= FileSystem::NMOUNT; i++)
	{
		/* 获取当前装配块 */
		Mount *pMount = &(this->m_Mount[i]);

		/* 找到内存Inode对应的Mount装配块 */
		if (pMount->m_inodep == pInode)
		{
			/* 返回该装配块 */
			return pMount;
		}
	}
	return NULL; /* 查找失败 */
}

/*检查设备dev上编号blkno的磁盘块是否属于数据盘块区*/
bool FileSystem::BadBlock(SuperBlock *spb, short dev, int blkno)
{
	return 0;
}
