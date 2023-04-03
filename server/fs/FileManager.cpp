/*
 * @Description: this file defines the class FileManager
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */
#include "FileManager.h"
#include "Kernel.h"
#include "Utility.h"
#include "TimeInterrupt.h"

/*==========================class FileManager===============================*/

/*
 * 文件管理类(FileManager)
 * 封装了文件系统的各种系统调用在核心态下处理过程，
 * 如对文件的Open()、Close()、Read()、Write()等等
 * 封装了对文件系统访问的具体细节。
 */

/* 构造函数 */
FileManager::FileManager()
{
	// nothing to do here
}

/* 析构函数 */
FileManager::~FileManager()
{
	// nothing to do here
}

/* 初始化对全局对象*/
void FileManager::Initialize()
{
	this->m_FileSystem = &Kernel::Instance().GetFileSystem(); /* 获取文件系统对象地址 */

	this->m_InodeTable = &g_InodeTable;		  /* 获取内存i节点表对象地址 */
	this->m_OpenFileTable = &g_OpenFileTable; /* 获取打开文件表对象地址 */

	this->m_InodeTable->Initialize(); /* 初始化内存i节点表 */
}

/*
 * 功能：打开文件
 * 效果：建立打开文件结构，内存i节点开锁 、i_count 为正数（i_count ++）
 * */
void FileManager::Open()
{
	Inode *pInode;							/* 指向内存索引节点的指针 */
	User &u = Kernel::Instance().GetUser(); /* 获取用户进程对象 */

	pInode = this->NameI(NextChar, FileManager::OPEN); /* 0 = Open, not create */
	/* 没有找到相应的Inode */
	if (NULL == pInode)
	{
		return; /* NameI()已经设置了u.u_error，直接返回 */
	}
	this->Open1(pInode, u.u_arg[1], 0);
}

/*
 * 功能：创建一个新的文件
 * 效果：建立打开文件结构，内存i节点开锁 、i_count 为正数（应该是 1）
 * */
void FileManager::Creat()
{
	Inode *pInode;																		 /* 指向内存索引节点的指针 */
	User &u = Kernel::Instance().GetUser();												 /* 获取用户进程对象 */
	unsigned int newACCMode = u.u_arg[1] & (Inode::IRWXU | Inode::IRWXG | Inode::IRWXO); /*新文件的访问权限*/

	/* 搜索目录的模式为1，表示创建；若父目录不可写，出错返回 */
	pInode = this->NameI(NextChar, FileManager::CREATE);
	/* 没有找到相应的Inode，或NameI出错 */
	if (NULL == pInode)
	{
		if (u.u_error)
			return;
		/* 创建Inode */
		pInode = this->MakNode(newACCMode & (~Inode::ISVTX));
		/* 创建失败 */
		if (NULL == pInode)
		{
			return;
		}

		/*
		 * 如果所希望的名字不存在，使用参数trf = 2来调用open1()。
		 * 不需要进行权限检查，因为刚刚建立的文件的权限和传入参数mode
		 * 所表示的权限内容是一样的。
		 */
		this->Open1(pInode, File::FWRITE, 2);
	}
	else
	{
		/* 如果NameI()搜索到已经存在要创建的文件，则清空该文件（用算法ITrunc()）。UID没有改变
		 * 原来UNIX的设计是这样：文件看上去就像新建的文件一样。然而，新文件所有者和许可权方式没变。
		 * 也就是说creat指定的RWX比特无效。
		 * 邓蓉认为这是不合理的，应该改变。
		 * 现在的实现：creat指定的RWX比特有效 */
		this->Open1(pInode, File::FWRITE, 1);
		pInode->i_mode |= newACCMode;
	}
}

/*
 * trf == 0由open调用
 * trf == 1由creat调用，creat文件的时候搜索到同文件名的文件
 * trf == 2由creat调用，creat文件的时候未搜索到同文件名的文件，这是文件创建时更一般的情况
 * mode参数：打开文件模式，表示文件操作是 读、写还是读写
 */
void FileManager::Open1(Inode *pInode, int mode, int trf)
{
	User &u = Kernel::Instance().GetUser(); /* 获取用户进程对象 */

	/*
	 * 对所希望的文件已存在的情况下，即trf == 0或trf == 1进行权限检查
	 * 如果所希望的名字不存在，即trf == 2，不需要进行权限检查，因为刚建立
	 * 的文件的权限和传入的参数mode的所表示的权限内容是一样的。
	 */
	if (trf != 2)
	{
		/* 不为2，检查文件类型 */
		if (mode & File::FREAD)
		{
			/* 检查读权限 */
			this->Access(pInode, Inode::IREAD);
		}
		if (mode & File::FWRITE)
		{
			/* 检查写权限 */
			this->Access(pInode, Inode::IWRITE);
			/* 系统调用去写目录文件是不允许的 */
			if ((pInode->i_mode & Inode::IFMT) == Inode::IFDIR)
			{
				/* 设置错误号为EISDIR */
				u.u_error = User::EISDIR;
			}
		}
	}

	/* 检查是否有错误 */
	if (u.u_error)
	{
		/* 有错误，释放内存i节点 */
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* 在creat文件的时候搜索到同文件名的文件，释放该文件所占据的所有盘块 */
	if (1 == trf)
	{
		/* 清空文件内容 */
		pInode->ITrunc();
	}

	/* 解锁inode!
	 * 线性目录搜索涉及大量的磁盘读写操作，期间进程会入睡。
	 * 因此，进程必须上锁操作涉及的i节点。这就是NameI中执行的IGet上锁操作。
	 * 行至此，后续不再有可能会引起进程切换的操作，可以解锁i节点。
	 */
	pInode->Prele();

	/* 分配打开文件控制块File结构 */
	File *pFile = this->m_OpenFileTable->FAlloc();
	/* 分配失败 */
	if (NULL == pFile)
	{
		/* 释放内存i节点 */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 设置打开文件方式，建立File结构和内存Inode的勾连关系 */
	pFile->f_flag = mode & (File::FREAD | File::FWRITE); /* 设置打开文件方式 */
	pFile->f_inode = pInode;							 /* 建立File结构和内存Inode的勾连关系 */

	/* 特殊设备打开函数 */
	pInode->OpenI(mode & File::FWRITE);

	/* 为打开或者创建文件的各种资源都已成功分配，函数返回 */
	if (u.u_error == 0)
	{
		return;
	}
	else /* 如果出错则释放资源 */
	{
		/* 释放打开文件描述符 */
		int fd = u.u_ar0[User::EAX];
		if (fd != -1)
		{
			/* 释放打开文件描述符fd，递减File结构引用计数 */
			u.u_ofiles.SetF(fd, NULL);
			/* 递减File结构和Inode的引用计数 ,File结构没有锁 f_count为0就是释放File结构了*/
			pFile->f_count--;
		}
		/* 释放内存i节点 */
		this->m_InodeTable->IPut(pInode);
	}
}

/*本函数用以完成Close()系统调用处理过程*/
void FileManager::Close()
{
	User &u = Kernel::Instance().GetUser(); /* 获取用户进程对象 */
	int fd = u.u_arg[0];					/* 获取文件描述符 */

	/* 获取打开文件控制块File结构 */
	File *pFile = u.u_ofiles.GetF(fd);
	if (NULL == pFile)
	{
		return;
	}

	/* 释放打开文件描述符fd，递减File结构引用计数 */
	u.u_ofiles.SetF(fd, NULL);
	/* 递减File结构和Inode的引用计数 ,File结构没有锁 f_count为0就是释放File结构了*/
	this->m_OpenFileTable->CloseF(pFile);
}

/* 本函数用以完成Seek()系统调用处理过程 */
void FileManager::Seek()
{
	File *pFile;							/* 文件指针 */
	User &u = Kernel::Instance().GetUser(); /* 获取用户进程对象 */
	int fd = u.u_arg[0];					/* 获取文件描述符 */

	pFile = u.u_ofiles.GetF(fd); /* 获取打开文件控制块File结构 */
	if (NULL == pFile)
	{
		return; /* 若FILE不存在，GetF有设出错码 */
	}

	/* 管道文件不允许seek */
	if (pFile->f_flag & File::FPIPE)
	{
		u.u_error = User::ESPIPE; /* 设置错误号为ESPIPE */
		return;
	}

	int offset = u.u_arg[1]; /* 获取偏移量 */

	/* 如果u.u_arg[2]在3 ~ 5之间，那么长度单位由字节变为512字节 */
	if (u.u_arg[2] > 2)
	{
		offset = offset << 9; /* 乘以512 */
		u.u_arg[2] -= 3;	  /* arg 2 减去3 */
	}

	switch (u.u_arg[2])
	{
	/* 读写位置设置为offset */
	case 0:
		pFile->f_offset = offset;
		break;
	/* 读写位置加offset(可正可负) */
	case 1:
		pFile->f_offset += offset;
		break;
	/* 读写位置调整为文件长度加offset */
	case 2:
		pFile->f_offset = pFile->f_inode->i_size + offset;
		break;
	}
}

/* 本函数用以完成Dup()系统调用处理过程 */
void FileManager::Dup()
{
	File *pFile;							/* 文件指针 */
	User &u = Kernel::Instance().GetUser(); /* 获取用户进程对象 */
	int fd = u.u_arg[0];					/* 获取文件描述符 */

	pFile = u.u_ofiles.GetF(fd); /* 获取打开文件控制块File结构 */
	if (NULL == pFile)
	{
		/* 打开失败*/
		return;
	}

	int newFd = u.u_ofiles.AllocFreeSlot(); /* 分配新描述符 */
	if (newFd < 0)
	{
		/* 分配失败 */
		return;
	}
	/* 至此分配新描述符newFd成功 */
	u.u_ofiles.SetF(newFd, pFile);
	pFile->f_count++; /* 增加File结构引用计数 */
}

/* 本函数用以完成获取文件信息处理过程 */
void FileManager::FStat()
{
	File *pFile;							/* 文件指针 */
	User &u = Kernel::Instance().GetUser(); /* 获取用户进程对象 */
	int fd = u.u_arg[0];					/* 获取文件描述符 */

	pFile = u.u_ofiles.GetF(fd); /* 获取打开文件控制块File结构 */
	if (NULL == pFile)
	{
		/* 打开失败*/
		return;
	}

	/* u.u_arg[1] = pStatBuf */
	this->Stat1(pFile->f_inode, u.u_arg[1]); /* 获取文件信息 */
}

/* 本函数用以完成获取文件信息处理过程 */
void FileManager::Stat()
{
	Inode *pInode;							/* 文件指针 */
	User &u = Kernel::Instance().GetUser(); /* 获取用户进程对象 */

	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN); /* 获取文件信息 */
	if (NULL == pInode)
	{
		/* 打开失败*/
		return;
	}
	this->Stat1(pInode, u.u_arg[1]);  /* 获取文件信息 */
	this->m_InodeTable->IPut(pInode); /* 释放Inode */
}

/* 本函数系统调用的共享例程 */
void FileManager::Stat1(Inode *pInode, unsigned long statBuf)
{
	Buf *pBuf;													   /* 缓冲区指针 */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* 获取缓冲区管理对象 */

	pInode->IUpdate(Time::time);																									  /* 更新Inode的时间戳 */
	pBuf = bufMgr.Bread(pInode->i_dev, FileSystem::INODE_ZONE_START_SECTOR + pInode->i_number / FileSystem::INODE_NUMBER_PER_SECTOR); /* 读取外存Inode所在的扇区 */

	/* 将p指向缓存区中编号为inumber外存Inode的偏移位置 */
	unsigned char *p = pBuf->b_addr + (pInode->i_number % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
	Utility::DWordCopy((int *)p, (int *)statBuf, sizeof(DiskInode) / sizeof(int)); /* 复制Inode信息到用户空间 */

	bufMgr.Brelse(pBuf); /* 释放缓冲区 */
}

/*本函数用于Read()系统调用处理过程*/
void FileManager::Read()
{
	/* 直接调用Rdwr()函数即可 */
	this->Rdwr(File::FREAD);
}
/*本函数用于Write()系统调用处理过程*/
void FileManager::Write()
{
	/* 直接调用Rdwr()函数即可 */
	this->Rdwr(File::FWRITE);
}

/* 本函数用以完成读写系统调用公共部分代码 */
void FileManager::Rdwr(enum File::FileFlags mode)
{
	File *pFile;							/* 文件指针 */
	User &u = Kernel::Instance().GetUser(); /* 获取用户进程对象 */

	/* 根据Read()/Write()的系统调用参数fd获取打开文件控制块结构 */
	pFile = u.u_ofiles.GetF(u.u_arg[0]); /* fd */
	if (NULL == pFile)
	{
		/* 不存在该打开文件，GetF已经设置过出错码，所以这里不需要再设置了 */
		/*	u.u_error = User::EBADF;	*/
		return;
	}

	/* 读写的模式不正确 */
	if ((pFile->f_flag & mode) == 0)
	{
		u.u_error = User::EACCES; /* 无访问权限 */
		return;
	}

	u.u_IOParam.m_Base = (unsigned char *)u.u_arg[1]; /* 目标缓冲区首址 */
	u.u_IOParam.m_Count = u.u_arg[2];				  /* 要求读/写的字节数 */
	u.u_segflg = 0;									  /* User Space I/O，读入的内容要送数据段或用户栈段 */

	/* 管道读写 */
	if (pFile->f_flag & File::FPIPE)
	{
		/* 读写管道，不需要互斥访问，因为管道是全双工的，读写是互斥的。 */
		if (File::FREAD == mode)
		{
			/* 读管道 */
			this->ReadP(pFile);
		}
		else
		{
			/* 写管道 */
			this->WriteP(pFile);
		}
	}
	else
	/* 普通文件读写 ，或读写特殊文件。对文件实施互斥访问，互斥的粒度：每次系统调用。
	为此Inode类需要增加两个方法：NFlock()、NFrele()。
	这不是V6的设计。read、write系统调用对内存i节点上锁是为了给实施IO的进程提供一致的文件视图。*/
	{
		pFile->f_inode->NFlock(); /* 上锁 */
		/* 设置文件起始读位置 */
		u.u_IOParam.m_Offset = pFile->f_offset;
		if (File::FREAD == mode)
		{
			/* 读文件 */
			pFile->f_inode->ReadI();
		}
		else
		{
			/* 写文件 */
			pFile->f_inode->WriteI();
		}

		/* 根据读写字数，移动文件读写偏移指针 */
		pFile->f_offset += (u.u_arg[2] - u.u_IOParam.m_Count);
		pFile->f_inode->NFrele();
	}

	/* 返回实际读写的字节数，修改存放系统调用返回值的核心栈单元 */
	u.u_ar0[User::EAX] = u.u_arg[2] - u.u_IOParam.m_Count;
}

/* 本函数用于Pipe()管道建立系统调用处理过程*/
void FileManager::Pipe()
{
	Inode *pInode;							/* 用于创建管道文件的Inode */
	File *pFileRead;						/* 读管道的File结构 */
	File *pFileWrite;						/* 写管道的File结构 */
	int fd[2];								/*为读写管道的file结构分配的打开文件描述符*/
	User &u = Kernel::Instance().GetUser(); /* 获取当前进程的User结构 */

	/* 分配一个Inode用于创建管道文件 */
	pInode = this->m_FileSystem->IAlloc(DeviceManager::ROOTDEV);
	if (NULL == pInode)
	{
		/* 分配失败，设置出错码 */
		return;
	}

	/* 分配读管道的File结构 */
	pFileRead = this->m_OpenFileTable->FAlloc();
	if (NULL == pFileRead)
	{
		/* 分配失败，擦除管道文件的Inode */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 读管道的打开文件描述符，通过EAX获得打开文件描述符 */
	fd[0] = u.u_ar0[User::EAX];

	/* 分配写管道的File结构 */
	pFileWrite = this->m_OpenFileTable->FAlloc();
	if (NULL == pFileWrite) /*若分配失败，擦除管道读端相关的所有打开文件结构*/
	{
		/* 分配失败，擦除管道文件的Inode */
		pFileRead->f_count = 0;			  /* 读管道的File结构的引用计数清零 */
		u.u_ofiles.SetF(fd[0], NULL);	  /* 读管道的File结构的打开文件描述符清零 */
		this->m_InodeTable->IPut(pInode); /* 擦除管道文件的Inode */
		return;							  /* 返回 */
	}

	/* 写管道的打开文件描述符 */
	fd[1] = u.u_ar0[User::EAX]; /* 通过EAX获得打开文件描述符 */

	/* Pipe(int* fd)的参数在u.u_arg[0]中，将分配成功的2个fd返回给用户态程序 */
	int *pFdarr = (int *)u.u_arg[0]; /* 读写管道的打开文件描述符数组 */
	pFdarr[0] = fd[0];				 /* 读管道的打开文件描述符 */
	pFdarr[1] = fd[1];				 /* 写管道的打开文件描述符 */

	/* 设置读、写管道File结构的属性 ，以后read、write系统调用需要这个标识*/
	pFileRead->f_flag = File::FREAD | File::FPIPE;	 /* 设置读管道的File结构的标识 */
	pFileRead->f_inode = pInode;					 /* 设置读管道的File结构的Inode指针 */
	pFileWrite->f_flag = File::FWRITE | File::FPIPE; /* 设置写管道的File结构的标识 */
	pFileWrite->f_inode = pInode;					 /* 设置写管道的File结构的Inode指针 */

	pInode->i_count = 2;						/* 设置管道文件的Inode的引用计数为2 */
	pInode->i_flag = Inode::IACC | Inode::IUPD; /* 设置管道文件的Inode的标识 */
	pInode->i_mode = Inode::IALLOC;				/* 设置管道文件的Inode的模式为管道文件 */
}

/*本函数用于管道读操作*/
void FileManager::ReadP(File *pFile)
{
	Inode *pInode = pFile->f_inode;			/* 获取管道文件的Inode */
	User &u = Kernel::Instance().GetUser(); /* 获取当前进程的User结构 */

loop:
	/* 对管道文件上锁保证互斥 ，在现在的V6版本普通文件的读写也采取这种非常保守的上锁方式*/
	pInode->Plock();

	/* 管道中没有数据可读取 。管道文件从尾部开始写，故i_size是写指针。*/
	if (pFile->f_offset == pInode->i_size)
	{
		if (pFile->f_offset != 0)
		{
			/* 读管道文件偏移量和管道文件大小重置为0 */
			pFile->f_offset = 0;
			pInode->i_size = 0;

			/* 如果置上IWRITE标志，则表示有进程正在等待写此管道，所以必须唤醒相应的进程。*/
			if (pInode->i_mode & Inode::IWRITE)
			{
				pInode->i_mode &= (~Inode::IWRITE);											   /* 清除IWRITE标志 */
				Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)(pInode + 1)); /* 唤醒所有等待写管道的进程,睡眠原因为pInode+1 */
			}
		}

		pInode->Prele(); /* 不解锁的话，写管道进程无法对管道实施操作。系统死锁 */

		/* 如果管道的读者、写者中已经有一方关闭，则返回 */
		if (pInode->i_count < 2)
		{
			return;
		}

		/* IREAD标志表示有进程等待读Pipe */
		pInode->i_mode |= Inode::IREAD;
		/* 睡眠当前进程，等待写管道进程唤醒 */
		u.u_procp->Sleep((unsigned long)(pInode + 2), ProcessManager::PPIPE); /* 睡眠原因为pInode+2 */
		goto loop;
	}

	/* 管道中有有可读取的数据 */
	u.u_IOParam.m_Offset = pFile->f_offset; /* 设置读取管道的偏移量 */
	pInode->ReadI();						/* 读取管道中的数据 */
	pFile->f_offset = u.u_IOParam.m_Offset; /* 更新读取管道的偏移量 */
	pInode->Prele();						/* 解锁管道文件 */
}

/*本函数用于管道写操作*/
void FileManager::WriteP(File *pFile)
{
	/* 获取管道文件的Inode */
	Inode *pInode = pFile->f_inode;
	/* 获取当前进程的User结构 */
	User &u = Kernel::Instance().GetUser();
	/* 获取要写入管道的数据长度 */
	int count = u.u_IOParam.m_Count;

loop:
	/* 对管道文件上锁保证互斥 ，在现在的V6版本普通文件的读写也采取这种非常保守的上锁方式*/
	pInode->Plock();

	/* 已完成所有数据写入管道，对管道unlock并返回 */
	if (0 == count)
	{
		pInode->Prele();		 /* 解锁管道文件 */
		u.u_IOParam.m_Count = 0; /* 设置写入管道的数据长度为0 */
		return;					 /* 返回 */
	}

	/* 管道读者进程已关闭读端、用信号SIGPIPE通知应用程序 */
	if (pInode->i_count < 2)
	{
		pInode->Prele();				   /* 解锁管道文件 */
		u.u_error = User::EPIPE;		   /* 设置错误码为管道已关闭 */
		u.u_procp->PSignal(User::SIGPIPE); /* 向当前进程发送信号SIGPIPE */
		return;							   /* 返回 */
	}

	/* 如果已经到达管道的底，则置上同步标志，睡眠等待 */
	if (Inode::PIPSIZ == pInode->i_size)
	{
		pInode->i_mode |= Inode::IWRITE;									  /* 设置IWRITE标志 */
		pInode->Prele();													  /* 解锁管道文件 */
		u.u_procp->Sleep((unsigned long)(pInode + 1), ProcessManager::PPIPE); /* 睡眠原因为pInode+1 */
		goto loop;
	}

	/* 将待写入数据尽可能多地写入管道 */
	u.u_IOParam.m_Offset = pInode->i_size;											 /* 设置写入管道的偏移量 */
	u.u_IOParam.m_Count = Utility::Min(count, Inode::PIPSIZ - u.u_IOParam.m_Offset); /* 设置写入管道的数据长度 */
	count -= u.u_IOParam.m_Count;													 /* 更新待写入管道的数据长度 */
	pInode->WriteI();																 /* 将数据写入管道 */
	pInode->Prele();																 /* 解锁管道文件 */

	/* 唤醒读管道进程 */
	if (pInode->i_mode & Inode::IREAD)
	{
		pInode->i_mode &= (~Inode::IREAD);											   /* 清除IREAD标志 */
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)(pInode + 2)); /* 唤醒所有等待读管道的进程,睡眠原因为pInode+2 */
	}
	goto loop;
}

/*该函数用于取得与文件路径名相对应的文件或目录的inode元素,返回上锁后的Inode*/
/* 返回NULL表示目录搜索失败，否则是根指针，指向文件的内存打开i节点 ，上锁的内存i节点  */
Inode *FileManager::NameI(char (*func)(), enum DirectorySearchMode mode)
{
	Inode *pInode;												   /* 指向当前搜索的目录的Inode */
	Buf *pBuf;													   /* 指向当前搜索的目录的缓冲区 */
	char curchar;												   /* 当前字符 */
	char *pChar;												   /* 指向当前字符 */
	int freeEntryOffset;										   /* 以创建文件模式搜索目录时，记录空闲目录项的偏移量 */
	User &u = Kernel::Instance().GetUser();						   /* 指向当前进程的User结构 */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* 指向缓冲区管理器 */

	/*
	 * 如果该路径是'/'开头的，从根目录开始搜索，
	 * 否则从进程当前工作目录开始搜索。
	 */
	pInode = u.u_cdir; /* 指向当前进程的当前工作目录 */
	if ('/' == (curchar = (*func)()))
	{
		pInode = this->rootDirInode; /* 指向根目录 */
	}

	/* 检查该Inode是否正在被使用，以及保证在整个目录搜索过程中该Inode不被释放 */
	this->m_InodeTable->IGet(pInode->i_dev, pInode->i_number);

	/* 允许出现////a//b 这种路径 这种路径等价于/a/b */
	while ('/' == curchar)
	{
		curchar = (*func)(); /* 获取下一个字符 */
	}
	/* 如果试图更改和删除当前目录文件则出错 */
	if ('\0' == curchar && mode != FileManager::OPEN)
	{
		u.u_error = User::ENOENT; /* 设置错误码为文件不存在 */
		goto out;				  /* 释放当前搜索到的目录文件Inode，并退出 */
	}

	/* 外层循环每次处理pathname中一段路径分量 */
	while (true)
	{
		/* 如果出错则释放当前搜索到的目录文件Inode，并退出 */
		if (u.u_error != User::NOERROR)
		{
			break; /* goto out; */
		}

		/* 整个路径搜索完毕，返回相应Inode指针。目录搜索成功返回。 */
		if ('\0' == curchar)
		{
			return pInode; /* 返回上锁后的Inode */
		}

		/* 如果要进行搜索的不是目录文件，释放相关Inode资源则退出 */
		if ((pInode->i_mode & Inode::IFMT) != Inode::IFDIR)
		{
			u.u_error = User::ENOTDIR; /* 设置错误码为不是目录文件 */
			break;					   /* goto out; */
		}

		/* 进行目录搜索权限检查,IEXEC在目录文件中表示搜索权限 */
		if (this->Access(pInode, Inode::IEXEC))
		{
			u.u_error = User::EACCES; /* 设置错误码为没有搜索权限 */
			break;					  /* 不具备目录搜索权限，goto out; */
		}

		/*
		 * 将Pathname中当前准备进行匹配的路径分量拷贝到u.u_dbuf[]中，
		 * 便于和目录项进行比较。
		 */
		pChar = &(u.u_dbuf[0]); /* 指向u.u_dbuf[]的首地址 */
		while ('/' != curchar && '\0' != curchar && u.u_error == User::NOERROR)
		{
			if (pChar < &(u.u_dbuf[DirectoryEntry::DIRSIZ]))
			{
				/* 将当前字符拷贝到u.u_dbuf[]中 */
				*pChar = curchar;
				pChar++;
			}
			curchar = (*func)(); /* 获取下一个字符 */
		}
		/* 将u_dbuf剩余的部分填充为'\0' */
		while (pChar < &(u.u_dbuf[DirectoryEntry::DIRSIZ]))
		{
			*pChar = '\0';
			pChar++;
		}

		/* 允许出现////a//b 这种路径 这种路径等价于/a/b */
		while ('/' == curchar)
		{
			curchar = (*func)(); /* 获取下一个字符,忽略多余/ */
		}

		if (u.u_error != User::NOERROR)
		{
			break; /* goto out; */
		}

		/* 内层循环部分对于u.u_dbuf[]中的路径名分量，逐个搜寻匹配的目录项 */
		u.u_IOParam.m_Offset = 0;
		/* 设置为目录项个数 ，含空白的目录项*/
		u.u_IOParam.m_Count = pInode->i_size / (DirectoryEntry::DIRSIZ + 4);
		freeEntryOffset = 0; /* 以创建文件模式搜索目录时，记录空闲目录项的偏移量 */
		pBuf = NULL;		 /* 指向缓冲区 */

		while (true)
		{
			/* 对目录项已经搜索完毕 */
			if (0 == u.u_IOParam.m_Count)
			{
				if (NULL != pBuf)
				{
					/* 释放缓冲区 */
					bufMgr.Brelse(pBuf);
				}
				/* 如果是创建新文件 */
				if (FileManager::CREATE == mode && curchar == '\0')
				{
					/* 判断该目录是否可写 */
					if (this->Access(pInode, Inode::IWRITE))
					{
						u.u_error = User::EACCES; /* 设置错误码为没有写权限 */
						goto out;				  /* Failed */
					}

					/* 将父目录Inode指针保存起来，以后写目录项WriteDir()函数会用到 */
					u.u_pdir = pInode;

					if (freeEntryOffset) /* 此变量存放了空闲目录项位于目录文件中的偏移量 */
					{
						/* 将空闲目录项偏移量存入u区中，写目录项WriteDir()会用到 */
						u.u_IOParam.m_Offset = freeEntryOffset - (DirectoryEntry::DIRSIZ + 4);
					}
					else /*问题：为何if分支没有置IUPD标志？  这是因为文件的长度没有变呀*/
					{
						pInode->i_flag |= Inode::IUPD; /* 设置IUPD标志，表示目录文件已修改 */
					}
					/* 找到可以写入的空闲目录项位置，NameI()函数返回 */
					return NULL;
				}

				/* 目录项搜索完毕而没有找到匹配项，释放相关Inode资源，并推出 */
				u.u_error = User::ENOENT; /* 设置错误码为没有找到匹配项 */
				goto out;				  /* Failed */
			}

			/* 已读完目录文件的当前盘块，需要读入下一目录项数据盘块 */
			if (0 == u.u_IOParam.m_Offset % Inode::BLOCK_SIZE)
			{
				if (NULL != pBuf)
				{
					/* 释放缓冲区 */
					bufMgr.Brelse(pBuf);
				}
				/* 计算要读的物理盘块号 */
				int phyBlkno = pInode->Bmap(u.u_IOParam.m_Offset / Inode::BLOCK_SIZE);
				/* 读取目录项数据盘块 */
				pBuf = bufMgr.Bread(pInode->i_dev, phyBlkno);
			}

			/* 没有读完当前目录项盘块，则读取下一目录项至u.u_dent */
			int *src = (int *)(pBuf->b_addr + (u.u_IOParam.m_Offset % Inode::BLOCK_SIZE));
			/* 将目录项拷贝到u.u_dent中 */
			Utility::DWordCopy(src, (int *)&u.u_dent, sizeof(DirectoryEntry) / sizeof(int));

			/* 更新偏移量和计数器 */
			u.u_IOParam.m_Offset += (DirectoryEntry::DIRSIZ + 4); /* 4字节是目录项中的ino */
			u.u_IOParam.m_Count--;								  /* 目录项个数减1 */

			/* 如果是空闲目录项，记录该项位于目录文件中偏移量 */
			if (0 == u.u_dent.m_ino)
			{
				if (0 == freeEntryOffset)
				{
					/* 记录空闲目录项的偏移量 */
					freeEntryOffset = u.u_IOParam.m_Offset;
				}
				/* 跳过空闲目录项，继续比较下一目录项 */
				continue;
			}

			int i;
			for (i = 0; i < DirectoryEntry::DIRSIZ; i++)
			{
				/* 比较pathname中当前路径分量和目录项中的文件名是否相同 */
				if (u.u_dbuf[i] != u.u_dent.m_name[i])
				{
					break; /* 匹配至某一字符不符，跳出for循环 */
				}
			}

			if (i < DirectoryEntry::DIRSIZ)
			{
				/* 不是要搜索的目录项，继续匹配下一目录项 */
				continue;
			}
			else
			{
				/* 目录项匹配成功，回到外层While(true)循环 */
				break;
			}
		}

		/*
		 * 从内层目录项匹配循环跳至此处，说明pathname中
		 * 当前路径分量匹配成功了，还需匹配pathname中下一路径
		 * 分量，直至遇到'\0'结束。
		 */
		if (NULL != pBuf)
		{
			/* 释放缓冲区 */
			bufMgr.Brelse(pBuf);
		}

		/* 如果是删除操作，则返回父目录Inode，而要删除文件的Inode号在u.u_dent.m_ino中 */
		if (FileManager::DELETE == mode && '\0' == curchar)
		{
			/* 如果对父目录没有写的权限 */
			if (this->Access(pInode, Inode::IWRITE))
			{
				u.u_error = User::EACCES; /* 设置错误码为没有权限 */
				break;					  /* goto out; */
			}
			return pInode; /* 返回父目录Inode */
		}

		/*
		 * 匹配目录项成功，则释放当前目录Inode，根据匹配成功的
		 * 目录项m_ino字段获取相应下一级目录或文件的Inode。
		 */
		short dev = pInode->i_dev;		  /* 记录当前目录Inode的设备号 */
		this->m_InodeTable->IPut(pInode); /* 释放当前目录Inode */
		pInode = this->m_InodeTable->IGet(dev, u.u_dent.m_ino);
		/* 回到外层While(true)循环，继续匹配Pathname中下一路径分量 */

		if (NULL == pInode) /* 获取失败 */
		{
			return NULL;
		}
	}
/* 从外层While(true)循环跳至此处，说明匹配失败 */
out:
	/* 释放当前目录Inode */
	this->m_InodeTable->IPut(pInode);
	return NULL;
}

/* 本函数用于获得pathname中的下一个字符，pathname是用户进程的u.u_dirp指针指向的字符串。 */
char FileManager::NextChar()
{
	/* 从内核中获取用户进程的User结构 */
	User &u = Kernel::Instance().GetUser();

	/* u.u_dirp指向pathname中的字符 */
	return *u.u_dirp++;
}

/* 由creat调用。
 * 为新创建的文件写新的i节点和新的目录项
 * 返回的pInode是上了锁的内存i节点，其中的i_count是 1。
 *
 * 在程序的最后会调用 WriteDir，在这里把属于自己的目录项写进父目录，修改父目录文件的i节点 、将其写回磁盘。
 *
 */
Inode *FileManager::MakNode(unsigned int mode)
{
	Inode *pInode;							/* 用于保存新创建文件的Inode */
	User &u = Kernel::Instance().GetUser(); /* 从内核中获取用户进程的User结构 */

	/* 分配一个空闲DiskInode，里面内容已全部清空 */
	pInode = this->m_FileSystem->IAlloc(u.u_pdir->i_dev);
	if (NULL == pInode)
	{
		/* 分配失败 */
		return NULL;
	}

	pInode->i_flag |= (Inode::IACC | Inode::IUPD); /* 设置IACC和IUPD标志 */
	pInode->i_mode = mode | Inode::IALLOC;		   /* 设置文件类型和IALLOC标志 */
	pInode->i_nlink = 1;						   /* 设置硬链接数为1 */
	pInode->i_uid = u.u_uid;					   /* 设置文件所有者 */
	pInode->i_gid = u.u_gid;					   /* 设置文件所属组 */
	/* 将目录项写入u.u_dent，随后写入目录文件 */
	this->WriteDir(pInode);
	return pInode; /* 返回新创建文件的Inode */
}
/* 本函数用于向父目录的目录文件写入一个目录项*/
void FileManager::WriteDir(Inode *pInode)
{
	/* 从内核中获取用户进程的User结构 */
	User &u = Kernel::Instance().GetUser();

	/* 设置目录项中Inode编号部分 */
	u.u_dent.m_ino = pInode->i_number;

	/* 设置目录项中pathname分量部分 */
	for (int i = 0; i < DirectoryEntry::DIRSIZ; i++)
	{
		/* 将pathname分量拷贝到目录项中 */
		u.u_dent.m_name[i] = u.u_dbuf[i];
	}

	u.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4; /* 设置读写字节数, 4是Inode编号的字节数 */
	u.u_IOParam.m_Base = (unsigned char *)&u.u_dent;  /* 设置读写缓冲区 */
	u.u_segflg = 1;									  /* 设置段寄存器选择符为用户数据段 */

	/* 将目录项写入父目录文件 */
	u.u_pdir->WriteI();					/* 调用Inode::WriteI() */
	this->m_InodeTable->IPut(u.u_pdir); /* 释放父目录Inode */
}

/*本函数用于设置当前工作路径*/
void FileManager::SetCurDir(char *pathname)
{
	/* 从内核中获取用户进程的User结构 */
	User &u = Kernel::Instance().GetUser();

	/* 路径不是从根目录'/'开始，则在现有u.u_curdir后面加上当前路径分量 */
	if (pathname[0] != '/')
	{
		/* 计算当前工作目录的长度 */
		int length = Utility::StringLength(u.u_curdir);
		if (u.u_curdir[length - 1] != '/')
		{
			/* 如果当前工作目录不是以'/'结尾，则在后面加上'/' */
			u.u_curdir[length] = '/';
			length++;
		}
		/* 将当前路径分量拷贝到u.u_curdir后面 */
		Utility::StringCopy(pathname, u.u_curdir + length);
	}
	else /* 如果是从根目录'/'开始，则取代原有工作目录 */
	{
		Utility::StringCopy(pathname, u.u_curdir);
	}
}

/*本函数为检查对文件或目录的搜索、访问权限，作为系统调用的辅助函数*/
/*
 * 返回值是0，表示拥有打开文件的权限；1表示没有所需的访问权限。文件未能打开的原因记录在u.u_error变量中。
 */
int FileManager::Access(Inode *pInode, unsigned int mode)
{
	/* 从内核中获取用户进程的User结构 */
	User &u = Kernel::Instance().GetUser();

	/* 对于写的权限，必须检查该文件系统是否是只读的 */
	if (Inode::IWRITE == mode)
	{
		/* 如果是只读文件系统，则返回EROFS错误 */
		if (this->m_FileSystem->GetFS(pInode->i_dev)->s_ronly != 0)
		{
			/* 读写只读文件系统 */
			u.u_error = User::EROFS;
			return 1;
		}
	}
	/*
	 * 对于超级用户，读写任何文件都是允许的
	 * 而要执行某文件时，必须在i_mode有可执行标志
	 */
	if (u.u_uid == 0)
	{
		/* 超级用户 */
		if (Inode::IEXEC == mode && (pInode->i_mode & (Inode::IEXEC | (Inode::IEXEC >> 3) | (Inode::IEXEC >> 6))) == 0)
		{
			/* 超级用户执行文件时，必须在i_mode有可执行标志 */
			u.u_error = User::EACCES;
			return 1; /* Permission Check Failed! */
		}
		return 0; /* Permission Check Succeed! */
	}
	if (u.u_uid != pInode->i_uid)
	{
		/* 如果不是文件所有者，则检查文件所属组 */
		mode = mode >> 3; /* 检查文件所属组 */
		if (u.u_gid != pInode->i_gid)
		{
			/* 如果不是文件所属组，则检查其他用户 */
			mode = mode >> 3;
		}
	}
	if ((pInode->i_mode & mode) != 0)
	{
		/* 如果文件的i_mode中有所需的访问权限，则返回0 */
		return 0;
	}

	/* 如果文件的i_mode中没有所需的访问权限，则返回1 */
	u.u_error = User::EACCES;
	return 1;
}

/*本函数为检查文件是否属于当前用户的函数*/
Inode *FileManager::Owner()
{
	Inode *pInode;							/* 用于存放文件Inode指针 */
	User &u = Kernel::Instance().GetUser(); /* 从内核中获取用户进程的User结构 */

	if ((pInode = this->NameI(NextChar, FileManager::OPEN)) == NULL)
	{
		/* 文件不存在 */
		return NULL;
	}

	if (u.u_uid == pInode->i_uid || u.SUser())
	{
		/* 如果是文件所有者或超级用户，则返回文件Inode指针 */
		return pInode;
	}

	/* 如果不是文件所有者也不是超级用户，则释放文件Inode并返回NULL */
	this->m_InodeTable->IPut(pInode);
	/* 文件不属于当前用户 */
	return NULL;
}

/* 本函数用于改变文件访问模式 */
void FileManager::ChMod()
{
	Inode *pInode;							/* 用于存放文件Inode指针 */
	User &u = Kernel::Instance().GetUser(); /* 从内核中获取用户进程的User结构 */
	unsigned int mode = u.u_arg[1];			/* 从系统调用参数中获取新的访问模式 */

	if ((pInode = this->Owner()) == NULL)
	{
		/* 文件不属于当前用户 */
		return;
	}
	/* clear i_mode字段中的ISGID, ISUID, ISTVX以及rwxrwxrwx这12比特 */
	pInode->i_mode &= (~0xFFF);
	/* 根据系统调用的参数重新设置i_mode字段 */
	pInode->i_mode |= (mode & 0xFFF);
	pInode->i_flag |= Inode::IUPD;

	/* 释放文件Inode */
	this->m_InodeTable->IPut(pInode);
	return;
}

/* 本函数用于改变文件的所有者 */
void FileManager::ChOwn()
{
	Inode *pInode;							/* 用于存放文件Inode指针 */
	User &u = Kernel::Instance().GetUser(); /* 从内核中获取用户进程的User结构 */
	short uid = u.u_arg[1];					/* 从系统调用参数中获取新的文件所有者 */
	short gid = u.u_arg[2];					/* 从系统调用参数中获取新的文件所属组 */

	/* 不是超级用户或者不是文件主则返回 */
	if (!u.SUser() || (pInode = this->Owner()) == NULL)
	{
		/* 文件不属于当前用户 */
		return;
	}
	pInode->i_uid = uid;		   /* 设置文件所有者 */
	pInode->i_gid = gid;		   /* 设置文件所属组 */
	pInode->i_flag |= Inode::IUPD; /* 设置文件Inode已修改标志 */

	this->m_InodeTable->IPut(pInode); /* 释放文件Inode */
}

/* 本函数用于改变当前工作目录 */
void FileManager::ChDir()
{
	Inode *pInode;							/* 用于存放文件Inode指针 */
	User &u = Kernel::Instance().GetUser(); /* 从内核中获取用户进程的User结构 */

	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN); /* 打开文件 */
	if (NULL == pInode)
	{
		/* 文件不存在 */
		return;
	}
	/* 搜索到的文件不是目录文件 */
	if ((pInode->i_mode & Inode::IFMT) != Inode::IFDIR)
	{
		/* 设置错误号为ENOTDIR */
		u.u_error = User::ENOTDIR;
		/* 释放文件Inode */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 判断有无执行权限 */
	if (this->Access(pInode, Inode::IEXEC))
	{
		/* 释放文件Inode */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 释放当前目录的Inode */
	this->m_InodeTable->IPut(u.u_cdir);
	/* 设置当前目录的Inode */
	u.u_cdir = pInode;
	/* 释放当前inode锁 */
	pInode->Prele();
	/* 设置当前工作目录 */
	this->SetCurDir((char *)u.u_arg[0] /* pathname */);
}

/* 本函数用于创建文件的异名引用 */
void FileManager::Link()
{
	Inode *pInode;							/* 用于存放文件Inode指针 */
	Inode *pNewInode;						/* 用于存放新文件Inode指针 */
	User &u = Kernel::Instance().GetUser(); /* 从内核中获取用户进程的User结构 */
	/* 打开文件，获取文件Inode指针 */
	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN);
	/* 打卡文件失败 */
	if (NULL == pInode)
	{
		/* 文件不存在 */
		return;
	}
	/* 链接的数量已经最大 */
	if (pInode->i_nlink >= 255)
	{
		/* 设置错误号为EMLINK */
		u.u_error = User::EMLINK;
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 对目录文件的链接只能由超级用户进行 */
	if ((pInode->i_mode & Inode::IFMT) == Inode::IFDIR && !u.SUser())
	{
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* 解锁现存文件Inode,以避免在以下搜索新文件时产生死锁 */
	pInode->i_flag &= (~Inode::ILOCK);
	/* 指向要创建的新路径newPathname */
	u.u_dirp = (char *)u.u_arg[1];
	pNewInode = this->NameI(FileManager::NextChar, FileManager::CREATE);
	/* 如果文件已存在 */
	if (NULL != pNewInode)
	{
		/* 设置错误号为EEXIST */
		u.u_error = User::EEXIST;
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pNewInode);
	}
	if (User::NOERROR != u.u_error)
	{
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 检查目录与该文件是否在同一个设备上 */
	if (u.u_pdir->i_dev != pInode->i_dev)
	{
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(u.u_pdir);
		/* 设置错误号为EXDEV */
		u.u_error = User::EXDEV;
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* 写入目录项 */
	this->WriteDir(pInode);
	/* 更新目标文件的link数 */
	pInode->i_nlink++;
	/* 设置文件Inode已修改标志 */
	pInode->i_flag |= Inode::IUPD;
	/* 释放目录文件Inode */
	this->m_InodeTable->IPut(pInode);
}

/* 本函数用于删除文件的异名引用 */
void FileManager::UnLink()
{
	Inode *pInode;							/* 用于存放文件Inode指针 */
	Inode *pDeleteInode;					/* 用于存放要删除的文件Inode指针 */
	User &u = Kernel::Instance().GetUser(); /* 从内核中获取用户进程的User结构 */

	pDeleteInode = this->NameI(FileManager::NextChar, FileManager::DELETE); /* 打开文件，获取文件Inode指针 */
	if (NULL == pDeleteInode)
	{
		/* 文件不存在 */
		return;
	}
	pDeleteInode->Prele(); /* 解锁文件Inode */

	pInode = this->m_InodeTable->IGet(pDeleteInode->i_dev, u.u_dent.m_ino); /* 获取目录文件Inode */
	if (NULL == pInode)
	{
		Utility::Panic("unlink -- iget"); /* 获取失败，内核出错 */
	}
	/* 只有root可以unlink目录文件 */
	if ((pInode->i_mode & Inode::IFMT) == Inode::IFDIR && !u.SUser())
	{
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pDeleteInode);
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 写入清零后的目录项 */
	u.u_IOParam.m_Offset -= (DirectoryEntry::DIRSIZ + 4); /* 重新设置偏移量 */
	u.u_IOParam.m_Base = (unsigned char *)&u.u_dent;	  /* 重新设置基地址 */
	u.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;	  /* 重新设置字节数 */

	u.u_dent.m_ino = 0;		/* 清零目录项中的inode号 */
	pDeleteInode->WriteI(); /* 写入目录项 */

	/* 修改inode项 */
	pInode->i_nlink--;			   /* 链接数减1 */
	pInode->i_flag |= Inode::IUPD; /* 设置文件Inode已修改标志 */

	this->m_InodeTable->IPut(pDeleteInode); /* 释放文件Inode */
	this->m_InodeTable->IPut(pInode);		/* 释放目录文件Inode */
}

/* 用于建立特殊设备文件的系统调用 */
void FileManager::MkNod()
{
	Inode *pInode;							/* 用于存放文件Inode指针 */
	User &u = Kernel::Instance().GetUser(); /* 从内核中获取用户进程的User结构 */

	/* 检查uid是否是root，该系统调用只有uid==root时才可被调用 */
	if (u.SUser())
	{
		/* 指向要创建的新路径newPathname */
		pInode = this->NameI(FileManager::NextChar, FileManager::CREATE);
		/* 要创建的文件已经存在,这里并不能去覆盖此文件 */
		if (pInode != NULL)
		{
			u.u_error = User::EEXIST;		  /* 设置错误号为EEXIST */
			this->m_InodeTable->IPut(pInode); /* 释放文件Inode */
			return;							  /* 退出 */
		}
	}
	else
	{
		/* 非root用户执行mknod()系统调用返回User::EPERM */
		u.u_error = User::EPERM;
		return;
	}
	/* 没有通过SUser()的检查 */
	if (User::NOERROR != u.u_error)
	{
		return; /* 没有需要释放的资源，直接退出 */
	}
	/* 获取文件Inode */
	pInode = this->MakNode(u.u_arg[1]);
	if (NULL == pInode)
	{
		/* 获取失败，内核出错 */
		return;
	}
	/* 所建立是设备文件 */
	if ((pInode->i_mode & (Inode::IFBLK | Inode::IFCHR)) != 0)
	{
		/* 由于是特殊设备文件，所以i_addr[0]存放的是设备号 */
		pInode->i_addr[0] = u.u_arg[2];
	}
	/* 释放文件Inode */
	this->m_InodeTable->IPut(pInode);
}
/*==========================class DirectoryEntry===============================*/
/* 用于初始化目录项 */
DirectoryEntry::DirectoryEntry()
{
	/* 初始化目录项 */
	this->m_ino = 0;
	/* 初始化目录项名字 */
	this->m_name[0] = '\0';
}

DirectoryEntry::~DirectoryEntry()
{
	// nothing to do here
}
