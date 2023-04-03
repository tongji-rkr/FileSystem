/*
 * @Description: this file defines the class Inode
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */
#include "INode.h"
#include "Utility.h"
#include "DeviceManager.h"
#include "Kernel.h"

/*==============================class Inode===================================*/
/*	预读块的块号，对普通文件这是预读块所在的物理块号。对硬盘而言，这是当前物理块（扇区）的下一个物理块（扇区）*/
int Inode::rablock = 0;

/* 内存打开 i节点*/
Inode::Inode()
{
	/* 清空Inode对象中的数据 */
	// this->Clean();
	/* 去除this->Clean();的理由：
	 * Inode::Clean()特定用于IAlloc()中清空新分配DiskInode的原有数据，
	 * 即旧文件信息。Clean()函数中不应当清除i_dev, i_number, i_flag, i_count,
	 * 这是属于内存Inode而非DiskInode包含的旧文件信息，而Inode类构造函数需要
	 * 将其初始化为无效值。
	 */

	/* 将Inode对象的成员变量初始化为无效值 */
	this->i_flag = 0;			 /* 无效化标志 */
	this->i_mode = 0;			 /* 无效化文件工作方式信息 */
	this->i_count = 0;			 /* 无效化引用计数 */
	this->i_nlink = 0;			 /* 无效化链接数 */
	this->i_dev = -1;			 /* 无效化设备号 */
	this->i_number = -1;		 /* 无效化i节点号 */
	this->i_uid = -1;			 /* 无效化用户ID */
	this->i_gid = -1;			 /* 无效化组ID */
	this->i_size = 0;			 /* 无效化文件大小 */
	this->i_lastr = -1;			 /* 无效化最后读取的物理块号 */
	for (int i = 0; i < 10; i++) /* 无效化物理块索引表 */
	{
		this->i_addr[i] = 0;
	}
}

Inode::~Inode()
{
	// nothing to do here
}

/* 本函数用于根据Inode对象中的物理磁盘块索引表，读取相应文件数据 */
void Inode::ReadI()
{
	int lbn;													   /* 文件逻辑块号 */
	int bn;														   /* lbn对应的物理盘块号 */
	int offset;													   /* 当前字符块内起始传送位置 */
	int nbytes;													   /* 传送至用户目标区字节数量 */
	short dev;													   /* 设备号 */
	Buf *pBuf;													   /* 缓冲区指针 */
	User &u = Kernel::Instance().GetUser();						   /* 获取当前用户User结构 */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* 获取缓冲区管理器 */
	DeviceManager &devMgr = Kernel::Instance().GetDeviceManager(); /* 获取设备管理器 */

	if (0 == u.u_IOParam.m_Count)
	{
		/* 需要读字节数为零，则返回 */
		return;
	}

	this->i_flag |= Inode::IACC; /* 设置访问标志，写回时需要修改访问时间 */

	/* 如果是字符设备文件 ，调用外设读函数*/
	if ((this->i_mode & Inode::IFMT) == Inode::IFCHR)
	{
		short major = Utility::GetMajor(this->i_addr[0]); /* 获取设备号中的主设备号 */

		devMgr.GetCharDevice(major).Read(this->i_addr[0]); /* 调用外设读函数 */
		return;											   /* 读完后返回 */
	}

	/* 一次一个字符块地读入所需全部数据，直至遇到文件尾 */
	while (User::NOERROR == u.u_error && u.u_IOParam.m_Count != 0)
	{
		/* 计算当前字符块在文件中的逻辑块号lbn，以及在字符块中的起始位置offset */
		lbn = bn = u.u_IOParam.m_Offset / Inode::BLOCK_SIZE;
		offset = u.u_IOParam.m_Offset % Inode::BLOCK_SIZE;
		/* 传送到用户区的字节数量，取读请求的剩余字节数与当前字符块内有效字节数较小值 */
		nbytes = Utility::Min(Inode::BLOCK_SIZE - offset /* 块内有效字节数 */, u.u_IOParam.m_Count);

		if ((this->i_mode & Inode::IFMT) != Inode::IFBLK)
		{ /* 如果不是特殊块设备文件 */

			int remain = this->i_size - u.u_IOParam.m_Offset;
			/* 如果已读到超过文件结尾 */
			if (remain <= 0)
			{
				return; /* 读完后返回 */
			}
			/* 传送的字节数量还取决于剩余文件的长度 */
			nbytes = Utility::Min(nbytes, remain);

			/* 将逻辑块号lbn转换成物理盘块号bn ，Bmap有设置Inode::rablock。当UNIX认为获取预读块的开销太大时，
			 * 会放弃预读，此时 Inode::rablock 值为 0。
			 * */
			if ((bn = this->Bmap(lbn)) == 0)
			{
				/* 如果逻辑块号lbn对应的物理盘块号bn为0，则返回 */
				return;
			}
			dev = this->i_dev; /* 如果不是特殊块设备文件，则设备号为i_dev */
		}
		else /* 如果是特殊块设备文件 */
		{
			dev = this->i_addr[0];	 /* 特殊块设备文件i_addr[0]中存放的是设备号 */
			Inode::rablock = bn + 1; /* 预读下一块 */
		}

		if (this->i_lastr + 1 == lbn) /* 如果是顺序读，则进行预读 */
		{
			/* 读当前块，并预读下一块 */
			pBuf = bufMgr.Breada(dev, bn, Inode::rablock);
		}
		else
		{
			/* 并非顺序读，只读当前块 */
			pBuf = bufMgr.Bread(dev, bn); /* 读取物理盘块号bn对应的缓冲区 */
		}
		/* 记录最近读取字符块的逻辑块号 */
		this->i_lastr = lbn;

		/* 缓存中数据起始读位置 */
		unsigned char *start = pBuf->b_addr + offset;

		/* 读操作: 从缓冲区拷贝到用户目标区
		 * i386芯片用同一张页表映射用户空间和内核空间，这一点硬件上的差异 使得i386上实现 iomove操作
		 * 比PDP-11要容易许多*/
		Utility::IOMove(start, u.u_IOParam.m_Base, nbytes);

		/* 用传送字节数nbytes更新读写位置 */
		u.u_IOParam.m_Base += nbytes;	/* 更新用户目标区 */
		u.u_IOParam.m_Offset += nbytes; /* 更新读写位置 */
		u.u_IOParam.m_Count -= nbytes;	/* 更新剩余字节数 */

		bufMgr.Brelse(pBuf); /* 使用完缓存，释放该资源 */
	}
}

/*根据Inode对象中的物理磁盘块索引表，将数据写入文件*/
void Inode::WriteI()
{
	int lbn;													   /* 文件逻辑块号 */
	int bn;														   /* lbn对应的物理盘块号 */
	int offset;													   /* 当前字符块内起始传送位置 */
	int nbytes;													   /* 传送字节数量 */
	short dev;													   /* 设备号 */
	Buf *pBuf;													   /* 缓存指针 */
	User &u = Kernel::Instance().GetUser();						   /* 获取用户user数据结构 */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* 获取缓存管理器 */
	DeviceManager &devMgr = Kernel::Instance().GetDeviceManager(); /* 获取设备管理器 */

	/* 设置Inode被访问标志位 */
	this->i_flag |= (Inode::IACC | Inode::IUPD); /* 设置被访问标志位和被修改标志位 */

	/* 对字符设备的访问 */
	if ((this->i_mode & Inode::IFMT) == Inode::IFCHR)
	{
		/* 如果是字符设备文件，则直接调用字符设备的Write函数 */
		short major = Utility::GetMajor(this->i_addr[0]); /* 获取设备号 */

		devMgr.GetCharDevice(major).Write(this->i_addr[0]); /* 调用字符设备的Write函数 */
		return;												/* 返回 */
	}

	if (0 == u.u_IOParam.m_Count)
	{
		/* 需要读字节数为零，则返回 */
		return;
	}

	while (User::NOERROR == u.u_error && u.u_IOParam.m_Count != 0)
	{
		/* 计算当前字符块的逻辑块号lbn，以及在该字符块中的起始传送位置offset */
		lbn = u.u_IOParam.m_Offset / Inode::BLOCK_SIZE;
		offset = u.u_IOParam.m_Offset % Inode::BLOCK_SIZE;
		/* 计算传送字节数量nbytes,是该块中剩余大小与用户要求传送的大小的较小值 */
		nbytes = Utility::Min(Inode::BLOCK_SIZE - offset, u.u_IOParam.m_Count);

		if ((this->i_mode & Inode::IFMT) != Inode::IFBLK)
		{ /* 普通文件 */

			/* 将逻辑块号lbn转换成物理盘块号bn */
			if ((bn = this->Bmap(lbn)) == 0)
			{
				return; /* 如果逻辑块号lbn对应的物理盘块号bn为0，则返回 */
			}
			dev = this->i_dev; /* 设备号设为i_dev */
		}
		else
		{ /* 块设备文件，也就是硬盘 */
			dev = this->i_addr[0];
		}

		if (Inode::BLOCK_SIZE == nbytes)
		{
			/* 如果写入数据正好满一个字符块，则为其分配缓存 */
			pBuf = bufMgr.GetBlk(dev, bn);
		}
		else
		{
			/* 写入数据不满一个字符块，先读后写（读出该字符块以保护不需要重写的数据） */
			pBuf = bufMgr.Bread(dev, bn);
		}

		/* 缓存中数据的起始写位置 */
		unsigned char *start = pBuf->b_addr + offset;

		/* 写操作: 从用户目标区拷贝数据到缓冲区 */
		Utility::IOMove(u.u_IOParam.m_Base, start, nbytes);

		/* 用传送字节数nbytes更新读写位置,修改user结构中的IO参数结构 */
		u.u_IOParam.m_Base += nbytes;	/* 更新用户目标区 */
		u.u_IOParam.m_Offset += nbytes; /* 更新读写位置 */
		u.u_IOParam.m_Count -= nbytes;	/* 更新剩余字节数 */

		if (u.u_error != User::NOERROR) /* 写过程中出错 */
		{
			bufMgr.Brelse(pBuf); /* 释放缓存 */
		}
		else if ((u.u_IOParam.m_Offset % Inode::BLOCK_SIZE) == 0) /* 如果写满一个字符块 */
		{
			/* 以异步方式将字符块写入磁盘，进程不需等待I/O操作结束，可以继续往下执行 */
			bufMgr.Bawrite(pBuf);
		}
		else /* 如果缓冲区未写满 */
		{
			/* 将缓存标记为延迟写，不急于进行I/O操作将字符块输出到磁盘上 */
			bufMgr.Bdwrite(pBuf);
		}

		/* 普通文件长度增加 */
		if ((this->i_size < u.u_IOParam.m_Offset) && (this->i_mode & (Inode::IFBLK & Inode::IFCHR)) == 0)
		{
			/* 如果文件长度小于写入位置，则更新文件长度 */
			this->i_size = u.u_IOParam.m_Offset;
		}

		/*
		 * 之前过程中读盘可能导致进程切换，在进程睡眠期间当前内存Inode可能
		 * 被同步到外存Inode，在此需要重新设置更新标志位。
		 * 好像没有必要呀！即使write系统调用没有上锁，iput看到i_count减到0之后才会将内存i节点同步回磁盘。而这在
		 * 文件没有close之前是不会发生的。
		 * 我们的系统对write系统调用上锁就更不可能出现这种情况了。
		 * 真的想把它去掉。
		 */
		this->i_flag |= Inode::IUPD;
	}
}

/*本函数用于将文件的逻辑块号转换成对应的物理盘块号*/
int Inode::Bmap(int lbn)
{
	Buf *pFirstBuf;												   /* 用于访问一次间接索引表 */
	Buf *pSecondBuf;											   /* 用于访问二次间接索引表 */
	int phyBlkno;												   /* 转换后的物理盘块号 */
	int *iTable;												   /* 用于访问索引盘块中一次间接、两次间接索引表 */
	int index;													   /* 用于访问索引盘块中一次间接、两次间接索引表中的索引值 */
	User &u = Kernel::Instance().GetUser();						   /* 获取当前用户 */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* 获取缓冲区管理器 */
	FileSystem &fileSys = Kernel::Instance().GetFileSystem();	   /* 获取文件系统 */

	/*
	 * Unix V6++的文件索引结构：(小型、大型和巨型文件)
	 * (1) i_addr[0] - i_addr[5]为直接索引表，文件长度范围是0 - 6个盘块；
	 *
	 * (2) i_addr[6] - i_addr[7]存放一次间接索引表所在磁盘块号，每磁盘块
	 * 上存放128个文件数据盘块号，此类文件长度范围是7 - (128 * 2 + 6)个盘块；
	 *
	 * (3) i_addr[8] - i_addr[9]存放二次间接索引表所在磁盘块号，每个二次间接
	 * 索引表记录128个一次间接索引表所在磁盘块号，此类文件长度范围是
	 * (128 * 2 + 6 ) < size <= (128 * 128 * 2 + 128 * 2 + 6)
	 */

	if (lbn >= Inode::HUGE_FILE_BLOCK)
	{
		/* 如果文件长度超过巨型文件的最大长度，则返回错误 */
		u.u_error = User::EFBIG; /* 文件太大 */
		return 0;				 /* 返回0 */
	}

	if (lbn < 6) /* 如果是小型文件，从基本索引表i_addr[0-5]中获得物理盘块号即可 */
	{
		phyBlkno = this->i_addr[lbn]; /* 从基本索引表中获得物理盘块号 */

		/*
		 * 如果该逻辑块号还没有相应的物理盘块号与之对应，则分配一个物理块。
		 * 这通常发生在对文件的写入，当写入位置超出文件大小，即对当前
		 * 文件进行扩充写入，就需要分配额外的磁盘块，并为之建立逻辑块号
		 * 与物理盘块号之间的映射。
		 */
		if (phyBlkno == 0 && (pFirstBuf = fileSys.Alloc(this->i_dev)) != NULL)
		{
			/*
			 * 因为后面很可能马上还要用到此处新分配的数据块，所以不急于立刻输出到
			 * 磁盘上；而是将缓存标记为延迟写方式，这样可以减少系统的I/O操作。
			 */
			bufMgr.Bdwrite(pFirstBuf);	   /* 将新分配的数据块写入磁盘 */
			phyBlkno = pFirstBuf->b_blkno; /* 获得新分配的数据块的物理盘块号 */
			/* 将逻辑块号lbn映射到物理盘块号phyBlkno */
			this->i_addr[lbn] = phyBlkno; /* 将新分配的数据块的物理盘块号写入基本索引表 */
			this->i_flag |= Inode::IUPD;  /* 设置更新标志位 */
		}
		/* 找到预读块对应的物理盘块号 */
		Inode::rablock = 0;
		if (lbn <= 4)
		{
			/*
			 * i_addr[0] - i_addr[5]为直接索引表。如果预读块对应物理块号可以从
			 * 直接索引表中获得，则记录在Inode::rablock中。如果需要额外的I/O开销
			 * 读入间接索引块，就显得不太值得了。漂亮！
			 *
			 */
			Inode::rablock = this->i_addr[lbn + 1];
		}
		/* 此时如果文件非小型，则Inode::rablock为0，该参数将会在读写文件时被忽略 */
		return phyBlkno; /* 返回物理盘块号 */
	}
	else /* lbn >= 6 大型、巨型文件 */
	{
		/* 计算逻辑块号lbn对应i_addr[]中的索引 */

		if (lbn < Inode::LARGE_FILE_BLOCK) /* 大型文件: 长度介于7 - (128 * 2 + 6)个盘块之间 */
		{
			index = (lbn - Inode::SMALL_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK + 6; /* 计算索引 */
		}
		else /* 巨型文件: 长度介于263 - (128 * 128 * 2 + 128 * 2 + 6)个盘块之间 */
		{
			index = (lbn - Inode::LARGE_FILE_BLOCK) / (Inode::ADDRESS_PER_INDEX_BLOCK * Inode::ADDRESS_PER_INDEX_BLOCK) + 8; /* 计算索引 */
		}

		phyBlkno = this->i_addr[index];
		/* 若该项为零，则表示不存在相应的间接索引表块 */
		if (0 == phyBlkno)
		{
			this->i_flag |= Inode::IUPD;
			/* 分配一空闲盘块存放间接索引表 */
			if ((pFirstBuf = fileSys.Alloc(this->i_dev)) == NULL)
			{
				return 0; /* 分配失败 */
			}
			/* i_addr[index]中记录间接索引表的物理盘块号 */
			this->i_addr[index] = pFirstBuf->b_blkno;
		}
		else
		{
			/* 读出存储间接索引表的字符块 */
			pFirstBuf = bufMgr.Bread(this->i_dev, phyBlkno);
		}
		/* 获取缓冲区首址 */
		iTable = (int *)pFirstBuf->b_addr;

		if (index >= 8) /* ASSERT: 8 <= index <= 9 */
		{
			/*
			 * 对于巨型文件的情况，pFirstBuf中是二次间接索引表，
			 * 还需根据逻辑块号，经由二次间接索引表找到一次间接索引表
			 */
			index = ((lbn - Inode::LARGE_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK; /* 计算索引 */

			/* iTable指向缓存中的二次间接索引表。该项为零，不存在一次间接索引表 */
			phyBlkno = iTable[index]; /* 一次间接索引表的物理盘块号 */
			if (0 == phyBlkno)
			{
				/* 如果该一次间接索引表尚未被分配，分配一空闲盘块存放一次间接索引表 */
				if ((pSecondBuf = fileSys.Alloc(this->i_dev)) == NULL)
				{
					/* 分配一次间接索引表磁盘块失败，释放缓存中的二次间接索引表，然后返回 */
					bufMgr.Brelse(pFirstBuf);
					return 0;
				}
				/* 将新分配的一次间接索引表磁盘块号，记入二次间接索引表相应项 */
				iTable[index] = pSecondBuf->b_blkno;
				/* 将更改后的二次间接索引表延迟写方式输出到磁盘 */
				bufMgr.Bdwrite(pFirstBuf);
			}
			else
			{
				/* 释放二次间接索引表占用的缓存，并读入一次间接索引表 */
				bufMgr.Brelse(pFirstBuf);
				pSecondBuf = bufMgr.Bread(this->i_dev, phyBlkno);
			}

			/* 令pFirstBuf指向一次间接索引表 */
			pFirstBuf = pSecondBuf;
			/* 令iTable指向一次间接索引表 */
			iTable = (int *)pSecondBuf->b_addr;
		}

		/* 计算逻辑块号lbn最终位于一次间接索引表中的表项序号index */

		if (lbn < Inode::LARGE_FILE_BLOCK)
		{
			index = (lbn - Inode::SMALL_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK; /* 计算索引 */
		}
		else
		{
			index = (lbn - Inode::LARGE_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK; /* 计算索引 */
		}

		if ((phyBlkno = iTable[index]) == 0 && (pSecondBuf = fileSys.Alloc(this->i_dev)) != NULL)
		{
			/* 将分配到的文件数据盘块号登记在一次间接索引表中 */
			phyBlkno = pSecondBuf->b_blkno;
			iTable[index] = phyBlkno;
			/* 将数据盘块、更改后的一次间接索引表用延迟写方式输出到磁盘 */
			bufMgr.Bdwrite(pSecondBuf);
			bufMgr.Bdwrite(pFirstBuf);
		}
		else
		{
			/* 释放一次间接索引表占用缓存 */
			bufMgr.Brelse(pFirstBuf);
		}
		/* 找到预读块对应的物理盘块号，如果获取预读块号需要额外的一次for间接索引块的IO，不合算，放弃 */
		Inode::rablock = 0;
		/* 如果预读块号不是最后一个间接索引块，获取预读块号 */
		if (index + 1 < Inode::ADDRESS_PER_INDEX_BLOCK)
		{
			/* 读出存储间接索引表的字符块 */
			Inode::rablock = iTable[index + 1];
		}
		return phyBlkno; /* 返回预读块对应的物理盘块号 */
	}
}

/*对特殊字符设备、块设备文件，调用该设备注册在块设备开关表中的设备初始化程序*/
void Inode::OpenI(int mode)
{
	short dev;													   /* 设备号 */
	DeviceManager &devMgr = Kernel::Instance().GetDeviceManager(); /* 设备管理器 */
	User &u = Kernel::Instance().GetUser();						   /* 用户管理器 */

	/*
	 * 对于特殊块设备、字符设备文件，i_addr[]不再是
	 * 磁盘块号索引表，addr[0]中存放了设备号dev
	 */
	dev = this->i_addr[0]; /* 获取设备号 */

	/* 提取主设备号 */
	short major = Utility::GetMajor(dev); /* 提取主设备号 */

	/* 根据文件类型，调用相应的设备初始化程序 */
	switch (this->i_mode & Inode::IFMT)
	{
	case Inode::IFCHR: /* 字符设备特殊类型文件 */
		if (major >= devMgr.GetNChrDev())
		{
			u.u_error = User::ENXIO; /* no such device */
			return;
		}
		devMgr.GetCharDevice(major).Open(dev, mode);
		break;

	case Inode::IFBLK: /* 块设备特殊类型文件 */
		/* 检查设备号是否超出系统中块设备数量 */
		if (major >= devMgr.GetNBlkDev())
		{
			u.u_error = User::ENXIO; /* no such device */
			return;
		}
		/* 根据主设备号获取对应的块设备BlockDevice对象引用 */
		BlockDevice &bdev = devMgr.GetBlockDevice(major);
		/* 调用该设备的特定初始化逻辑 */
		bdev.Open(dev, mode);
		break;
	}

	return;
}

/* 对特殊字符设备、块设备文件。如果对该设备的引用计数为0，则调用该设备的关闭程序 */
void Inode::CloseI(int mode)
{
	short dev;													   /* 设备号 */
	DeviceManager &devMgr = Kernel::Instance().GetDeviceManager(); /* 设备管理器 */

	/* addr[0]中存放了设备号dev */
	dev = this->i_addr[0]; /* 获取设备号 */

	short major = Utility::GetMajor(dev); /* 提取主设备号 */

	/* 不再使用该文件,关闭特殊文件 */
	if (this->i_count <= 1)
	{
		/* 根据文件类型，调用相应的设备关闭程序 */
		switch (this->i_mode & Inode::IFMT)
		{
		case Inode::IFCHR:
			devMgr.GetCharDevice(major).Close(dev, mode); /* 调用该设备的特定关闭逻辑 */
			break;

		case Inode::IFBLK:
			/* 根据主设备号获取对应的块设备BlockDevice对象引用 */
			BlockDevice &bdev = devMgr.GetBlockDevice(major);
			/* 调用该设备的特定关闭逻辑 */
			bdev.Close(dev, mode);
			break;
		}
	}
}

/* 本函数用于更新外存Inode的最后的访问时间、修改时间*/
void Inode::IUpdate(int time)
{
	Buf *pBuf;													   /* 缓存块指针 */
	DiskInode dInode;											   /* 磁盘Inode */
	FileSystem &filesys = Kernel::Instance().GetFileSystem();	   /* 文件系统 */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* 缓存管理器 */

	/* 当IUPD和IACC标志之一被设置，才需要更新相应DiskInode
	 * 目录搜索，不会设置所途径的目录文件的IACC和IUPD标志 */
	if ((this->i_flag & (Inode::IUPD | Inode::IACC)) != 0)
	{
		if (filesys.GetFS(this->i_dev)->s_ronly != 0)
		{
			/* 如果该文件系统只读 */
			return;
		}

		/* 邓蓉的注释：在缓存池中找到包含本i节点（this->i_number）的缓存块
		 * 这是一个上锁的缓存块，本段代码中的Bwrite()在将缓存块写回磁盘后会释放该缓存块。
		 * 将该存放该DiskInode的字符块读入缓冲区 */
		pBuf = bufMgr.Bread(this->i_dev, FileSystem::INODE_ZONE_START_SECTOR + this->i_number / FileSystem::INODE_NUMBER_PER_SECTOR); /* 读入包含该Inode的缓存块 */

		/* 将内存Inode副本中的信息复制到dInode中，然后将dInode覆盖缓存中旧的外存Inode */
		dInode.d_mode = this->i_mode;	/* 文件类型和访问权限 */
		dInode.d_nlink = this->i_nlink; /* 链接数 */
		dInode.d_uid = this->i_uid;		/* 文件所有者的用户标识符 */
		dInode.d_gid = this->i_gid;		/* 文件所有者的组标识符 */
		dInode.d_size = this->i_size;	/* 文件长度 */
		for (int i = 0; i < 10; i++)
		{
			dInode.d_addr[i] = this->i_addr[i]; /* 文件占用的磁盘块号 */
		}
		if (this->i_flag & Inode::IACC)
		{
			/* 更新最后访问时间 */
			dInode.d_atime = time;
		}
		if (this->i_flag & Inode::IUPD)
		{
			/* 更新最后访问时间 */
			dInode.d_mtime = time;
		}

		/* 将p指向缓存区中旧外存Inode的偏移位置 */
		unsigned char *p = pBuf->b_addr + (this->i_number % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
		DiskInode *pNode = &dInode;

		/* 用dInode中的新数据覆盖缓存中的旧外存Inode */
		Utility::DWordCopy((int *)pNode, (int *)p, sizeof(DiskInode) / sizeof(int));

		/* 将缓存写回至磁盘，达到更新旧外存Inode的目的 */
		bufMgr.Bwrite(pBuf);
	}
}
/* 本函数用于释放Inode所占用的磁盘块 */
void Inode::ITrunc()
{
	/* 经由磁盘高速缓存读取存放一次间接、两次间接索引表的磁盘块 */
	BufferManager &bm = Kernel::Instance().GetBufferManager();
	/* 获取g_FileSystem对象的引用，执行释放磁盘块的操作 */
	FileSystem &filesys = Kernel::Instance().GetFileSystem();

	/* 如果是字符设备或者块设备则退出 */
	if (this->i_mode & (Inode::IFCHR & Inode::IFBLK))
	{
		return;
	}

	/* 采用FILO方式释放，以尽量使得SuperBlock中记录的空闲盘块号连续。
	 *
	 * Unix V6++的文件索引结构：(小型、大型和巨型文件)
	 * (1) i_addr[0] - i_addr[5]为直接索引表，文件长度范围是0 - 6个盘块；
	 *
	 * (2) i_addr[6] - i_addr[7]存放一次间接索引表所在磁盘块号，每磁盘块
	 * 上存放128个文件数据盘块号，此类文件长度范围是7 - (128 * 2 + 6)个盘块；
	 *
	 * (3) i_addr[8] - i_addr[9]存放二次间接索引表所在磁盘块号，每个二次间接
	 * 索引表记录128个一次间接索引表所在磁盘块号，此类文件长度范围是
	 * (128 * 2 + 6 ) < size <= (128 * 128 * 2 + 128 * 2 + 6)
	 */
	for (int i = 9; i >= 0; i--) /* 从i_addr[9]到i_addr[0] */
	{
		/* 如果i_addr[]中第i项存在索引 */
		if (this->i_addr[i] != 0)
		{
			/* 如果是i_addr[]中的一次间接、两次间接索引项 */
			if (i >= 6 && i <= 9)
			{
				/* 将间接索引表读入缓存 */
				Buf *pFirstBuf = bm.Bread(this->i_dev, this->i_addr[i]);
				/* 获取缓冲区首址 */
				int *pFirst = (int *)pFirstBuf->b_addr;

				/* 每张间接索引表记录 512/sizeof(int) = 128个磁盘块号，遍历这全部128个磁盘块 */
				for (int j = 128 - 1; j >= 0; j--)
				{
					if (pFirst[j] != 0) /* 如果该项存在索引 */
					{
						/*
						 * 如果是两次间接索引表，i_addr[8]或i_addr[9]项，
						 * 那么该字符块记录的是128个一次间接索引表存放的磁盘块号
						 */
						if (i >= 8 && i <= 9)
						{
							Buf *pSecondBuf = bm.Bread(this->i_dev, pFirst[j]); /* 将一次间接索引表读入缓存,通过循环遍历进行释放 */
							int *pSecond = (int *)pSecondBuf->b_addr;			/* 获取缓冲区首址 */

							for (int k = 128 - 1; k >= 0; k--)
							{
								if (pSecond[k] != 0)
								{
									/* 释放指定的磁盘块 */
									filesys.Free(this->i_dev, pSecond[k]);
								}
							}
							/* 缓存使用完毕，释放以便被其它进程使用 */
							bm.Brelse(pSecondBuf);
						}
						/* 释放指定的磁盘块 */
						filesys.Free(this->i_dev, pFirst[j]);
					}
				}
				/* 缓存使用完毕，释放以便被其它进程使用 */
				bm.Brelse(pFirstBuf);
			}
			/* 释放索引表本身占用的磁盘块 */
			filesys.Free(this->i_dev, this->i_addr[i]);
			/* 0表示该项不包含索引 */
			this->i_addr[i] = 0;
		}
	}

	/* 盘块释放完毕，文件大小清零 */
	this->i_size = 0;
	/* 增设IUPD标志位，表示此内存Inode需要同步到相应外存Inode */
	this->i_flag |= Inode::IUPD;
	/* 清大文件标志 和原来的RWXRWXRWX比特*/
	this->i_mode &= ~(Inode::ILARG & Inode::IRWXU & Inode::IRWXG & Inode::IRWXO);
	/* 释放内存Inode */
	this->i_nlink = 1;
}

/* 本函数用于对Pipe或者Inode解锁，并且唤醒因等待锁而睡眠的进程*/
void Inode::NFrele()
{
	/* 解锁pipe或Inode,并且唤醒相应进程 */
	this->i_flag &= ~Inode::ILOCK;

	/* 如果有进程在等待该Inode，则唤醒它们 */
	if (this->i_flag & Inode::IWANT)
	{
		/* 清除等待标志位 */
		this->i_flag &= ~Inode::IWANT;
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)this); /* 唤醒所有等待该Inode的进程 */
	}
}

/*对Pipe上锁，如果Pipe已经被上锁，则增设IWANT标志并睡眠等待直至解锁*/
void Inode::NFlock()
{
	User &u = Kernel::Instance().GetUser(); /* 获取当前进程的User结构 */

	/* 如果Pipe已经被上锁，则增设IWANT标志并睡眠等待直至解锁 */
	while (this->i_flag & Inode::ILOCK)
	{
		this->i_flag |= Inode::IWANT;								   /* 增设IWANT标志 */
		u.u_procp->Sleep((unsigned long)this, ProcessManager::PRIBIO); /* 睡眠等待直至解锁 */
	}
	this->i_flag |= Inode::ILOCK; /* 增设ILOCK标志 */
}

/* 本函数用于对Pipe解锁，并且唤醒因等待锁而睡眠的进程*/
void Inode::Prele()
{
	/* 解锁pipe或Inode,并且唤醒相应进程 */
	this->i_flag &= ~Inode::ILOCK;

	if (this->i_flag & Inode::IWANT)
	{
		/* 如果有进程在等待该Inode，则唤醒它们 */
		this->i_flag &= ~Inode::IWANT;										   /* 清除等待标志位 */
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)this); /* 唤醒所有等待该Inode的进程 */
	}
}

/*对Pipe上锁，如果Pipe已经被上锁，则增设IWANT标志并睡眠等待直至解锁*/
void Inode::Plock()
{
	/* 获取当前进程的User结构 */
	User &u = Kernel::Instance().GetUser();

	/* 如果Pipe已经被上锁，则增设IWANT标志并睡眠等待直至解锁 */
	while (this->i_flag & Inode::ILOCK)
	{
		this->i_flag |= Inode::IWANT; /* 增设IWANT标志 */
		u.u_procp->Sleep((unsigned long)this, ProcessManager::PPIPE);
		/* 睡眠等待直至解锁,注意此处的睡眠原因为PPIPE */
	}
	this->i_flag |= Inode::ILOCK; /* 增设ILOCK标志 */
}

/*本函数用于清空Inode对象中的数据，但不包括i_dev, i_number, i_flag, i_count*/
void Inode::Clean()
{
	/*
	 * Inode::Clean()特定用于IAlloc()中清空新分配DiskInode的原有数据，
	 * 即旧文件信息。Clean()函数中不应当清除i_dev, i_number, i_flag, i_count,
	 * 这是属于内存Inode而非DiskInode包含的旧文件信息，而Inode类构造函数需要
	 * 将其初始化为无效值。
	 */

	// this->i_flag = 0;
	this->i_mode = 0; /* 清空文件类型和访问权限 */
	// this->i_count = 0;
	this->i_nlink = 0; /* 清空文件链接数 */
	// this->i_dev = -1;
	// this->i_number = -1;
	this->i_uid = -1;	/* 清空文件所有者 */
	this->i_gid = -1;	/* 清空文件所有者组 */
	this->i_size = 0;	/* 清空文件大小 */
	this->i_lastr = -1; /* 清空最后一次读取的文件块号 */
	for (int i = 0; i < 10; i++)
	{
		this->i_addr[i] = 0; /* 清空文件数据块号 */
	}
}

/* 本函数用于将内存Inode中的信息拷贝到外存Inode中 */
void Inode::ICopy(Buf *bp, int inumber)
{
	DiskInode dInode;			/* 临时变量，用于存放外存Inode信息 */
	DiskInode *pNode = &dInode; /* 指向临时变量dInode */

	/* 将p指向缓存区中编号为inumber外存Inode的偏移位置 */
	unsigned char *p = bp->b_addr + (inumber % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
	/* 将缓存中外存Inode数据拷贝到临时变量dInode中，按4字节拷贝 */
	Utility::DWordCopy((int *)p, (int *)pNode, sizeof(DiskInode) / sizeof(int));

	/* 将外存Inode变量dInode中信息复制到内存Inode中 */
	this->i_mode = dInode.d_mode;	/* 文件类型和访问权限 */
	this->i_nlink = dInode.d_nlink; /* 文件链接数 */
	this->i_uid = dInode.d_uid;		/* 文件所有者 */
	this->i_gid = dInode.d_gid;		/* 文件所有者组 */
	this->i_size = dInode.d_size;	/* 文件大小 */
	for (int i = 0; i < 10; i++)
	{
		this->i_addr[i] = dInode.d_addr[i]; /* 文件数据块号 */
	}
}

/*============================class DiskInode=================================*/
/*
 * 外存索引节点(DiskINode)的定义
 * 外存Inode位于文件存储设备上的
 * 外存Inode区中。每个文件有唯一对应
 * 的外存Inode，其作用是记录了该文件
 * 对应的控制信息。
 * 外存Inode中许多字段和内存Inode中字段
 * 相对应。外存INode对象长度为64字节，
 * 每个磁盘块可以存放512/64 = 8个外存Inode
 */
/* 外存Inode构造函数用于初始化栈中内存空间 */
DiskInode::DiskInode()
{
	/*
	 * 如果DiskInode没有构造函数，会发生如下较难察觉的错误：
	 * DiskInode作为局部变量占据函数Stack Frame中的内存空间，但是
	 * 这段空间没有被正确初始化，仍旧保留着先前栈内容，由于并不是
	 * DiskInode所有字段都会被更新，将DiskInode写回到磁盘上时，可能
	 * 将先前栈内容一同写回，导致写回结果出现莫名其妙的数据。
	 * 因此需要在DiskInode构造函数中将所有字段初始化为无效值。
	 */
	this->d_mode = 0;  /* 文件类型和访问权限 */
	this->d_nlink = 0; /* 文件链接数 */
	this->d_uid = -1;  /* 文件所有者 */
	this->d_gid = -1;  /* 文件所有者组 */
	this->d_size = 0;  /* 文件大小 */
	for (int i = 0; i < 10; i++)
	{
		this->d_addr[i] = 0; /* 文件数据块号 */
	}
	this->d_atime = 0; /* 文件最后一次访问时间 */
	this->d_mtime = 0; /* 文件最后一次修改时间 */
}

DiskInode::~DiskInode()
{
	// nothing to do here
}
