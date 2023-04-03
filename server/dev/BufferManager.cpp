/*
 * @Description: this file defines the class BufferManager
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */

#include "BufferManager.h"
#include "Kernel.h"

BufferManager::BufferManager()
{
	// nothing to do here
}

BufferManager::~BufferManager()
{
	// nothing to do here
}
// 初始化
void BufferManager::Initialize()
{
	int i;
	Buf *bp;

	// 由于目前各个队列都是空的，所以让自由缓存控制块队列勾连指针都指向其自身
	this->bFreeList.b_forw = this->bFreeList.b_back = &(this->bFreeList);
	this->bFreeList.av_forw = this->bFreeList.av_back = &(this->bFreeList);

	for (i = 0; i < NBUF; i++)
	{
		bp = &(this->m_Buf[i]);
		bp->b_dev = -1;				  // 主设备号和次设备号初始化为所有位都由1组成
		bp->b_addr = this->Buffer[i]; // 指向所管理的缓冲区的首地址
		/* 初始化NODEV队列 */
		bp->b_back = &(this->bFreeList);	 // 指向自由队列的队尾
		bp->b_forw = this->bFreeList.b_forw; // 指向自由队列的队首
		this->bFreeList.b_forw->b_back = bp; // 自由队列的队首的前驱指向bp
		this->bFreeList.b_forw = bp;		 // 自由队列的队首指向bp
		/* 初始化自由队列 */
		bp->b_flags = Buf::B_BUSY; // 置忙标志
		Brelse(bp);				   // 解锁
	}
	// 指向设备管理模块全局对象
	this->m_DeviceManager = &Kernel::Instance().GetDeviceManager();
	return;
}

// 给缓存块上锁，锁住缓存池中，用来装<dev,blkno>的缓存块
Buf *BufferManager::GetBlk(short dev, int blkno)
{
	Buf *bp;
	Devtab *dp;
	User &u = Kernel::Instance().GetUser(); // 获取当前的user结构

	/* 如果主设备号超出了系统中块设备数量 */
	if (Utility::GetMajor(dev) >= this->m_DeviceManager->GetNBlkDev())
	{
		Utility::Panic("nblkdev: There doesn't exist the device"); // 输出错误信息
	}

	/*
	 * 如果设备队列中已经存在相应缓存，则返回该缓存；
	 * 否则从自由队列中分配新的缓存用于字符块读写。
	 */
loop:
	/* 表示请求NODEV设备中字符块 */
	if (dev < 0)
	{
		dp = (Devtab *)(&this->bFreeList);
	}
	else
	{
		short major = Utility::GetMajor(dev); // 主设备号
		/* 根据主设备号获得块设备表 */
		dp = this->m_DeviceManager->GetBlockDevice(major).d_tab;

		// 如果没有块设备表则需要报错
		if (dp == NULL)
		{
			Utility::Panic("Null devtab!");
		}
		/* 首先在该设备队列中搜索是否有相应的缓存 */
		for (bp = dp->b_forw; bp != (Buf *)dp; bp = bp->b_forw)
		{
			/* 不是要找的缓存，则继续 */
			if (bp->b_blkno != blkno || bp->b_dev != dev)
				continue;

			/*
			 * 临界区之所以要从这里开始，而不是从上面的for循环开始。
			 * 主要是因为，中断服务程序并不会去修改块设备表中的
			 * 设备buf队列(b_forw)，所以不会引起冲突。
			 */
			X86Assembly::CLI();			   // 关中断
			if (bp->b_flags & Buf::B_BUSY) // 如果目前该缓存块已经被使用了
			{
				bp->b_flags |= Buf::B_WANTED;								 // 该缓存块被等待使用
				u.u_procp->Sleep((unsigned long)bp, ProcessManager::PRIBIO); // 进程入睡
				X86Assembly::STI();											 // 开中断
				goto loop;													 // 继续循环
			}
			X86Assembly::STI(); // 开中断
			/* 从自由队列中抽取出来 */
			this->NotAvail(bp);
			return bp;
		}
	} // end of else
	/*如果运行到这里说明在该设备队列中没有找到对应的缓存，
	 * 即缓存不命中，那么就需要用一个自由缓存队列中的缓存块来存这个缓存
	 * */

	X86Assembly::CLI();
	/* 如果自由队列为空 */
	if (this->bFreeList.av_forw == &this->bFreeList)
	{
		this->bFreeList.b_flags |= Buf::B_WANTED;								   // 置等待标志
		u.u_procp->Sleep((unsigned long)&this->bFreeList, ProcessManager::PRIBIO); // 入睡
		X86Assembly::STI();														   // 开中断
		goto loop;																   // 入睡循环等待
	}
	X86Assembly::STI(); // 开中断

	/* 取自由队列第一个空闲块 */
	bp = this->bFreeList.av_forw;
	this->NotAvail(bp);

	/* 如果该字符块是延迟写，将其异步写到磁盘上 */
	if (bp->b_flags & Buf::B_DELWRI)
	{
		bp->b_flags |= Buf::B_ASYNC; // 置异步标志
		this->Bwrite(bp);			 // 异步写回去
		goto loop;					 // 循环
	}
	/* 注意: 这里清除了所有其他位，只设了B_BUSY */
	bp->b_flags = Buf::B_BUSY;

	/* 从原设备队列中抽出 */
	bp->b_back->b_forw = bp->b_forw; // 原来链表中该项前一项的后一项变成该项的后一项
	bp->b_forw->b_back = bp->b_back; // 原来链表中该项后一项的前一项变成该项的前一项
	/* 加入新的设备队列 */
	bp->b_forw = dp->b_forw;
	bp->b_back = (Buf *)dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;

	bp->b_dev = dev;	 // 修改该缓存块的设备号
	bp->b_blkno = blkno; // 修改该缓存块的块号
	return bp;
}

/*缓存使用完毕，进行释放*/
void BufferManager::Brelse(Buf *bp)
{
	ProcessManager &procMgr = Kernel::Instance().GetProcessManager(); // 获取当前的进程管理类

	// 如果缓存块bp还在被等待使用，唤醒等待该缓存块的进程
	if (bp->b_flags & Buf::B_WANTED)
	{
		procMgr.WakeUpAll((unsigned long)bp);
	}

	/* 如果有进程正在等待分配自由队列中的缓存，则唤醒相应进程（因为释放了现在这个进程，它就成自由的了） */
	if (this->bFreeList.b_flags & Buf::B_WANTED)
	{
		/* 清楚B_WANTED标志，表示已有空闲缓存 */
		this->bFreeList.b_flags &= (~Buf::B_WANTED);
		procMgr.WakeUpAll((unsigned long)&this->bFreeList); // 唤醒等待自由缓存的进程
	}

	/* 如果有错误发生，则将次设备号改掉，
	 * 避免后续进程误用缓冲区中的错误数据。
	 */
	if (bp->b_flags & Buf::B_ERROR)
	{
		Utility::SetMinor(bp->b_dev, -1);
	}

	/* 临界资源，比如：在同步读末期会调用这个函数，
	 * 此时很有可能会产生磁盘中断，同样会调用这个函数。
	 */
	X86Assembly::CLI(); /* spl6();  UNIX V6的做法 */

	/* 注意以下操作并没有清除B_DELWRI、B_WRITE、B_READ、B_DONE标志
	 * B_DELWRI表示虽然将该控制块释放到自由队列里面，但是有可能还没有些到磁盘上。
	 * B_DONE则是指该缓存的内容正确地反映了存储在或应存储在磁盘上的信息
	 */
	bp->b_flags &= ~(Buf::B_WANTED | Buf::B_BUSY | Buf::B_ASYNC); // 清除B_WANTED，B_BUSY，B_ASYNC
	(this->bFreeList.av_back)->av_forw = bp;					  // 把bp插到自由缓存队列的队尾
	bp->av_back = this->bFreeList.av_back;						  // bp的前一个缓存块就成原来自由缓存队列队尾的缓存块
	bp->av_forw = &(this->bFreeList);							  // bp后面没有别的缓存块了
	this->bFreeList.av_back = bp;								  // bp成为了自由缓存队列的队尾

	X86Assembly::STI(); // 开中断
	return;
}

/*进程入睡等等待IO完成*/
void BufferManager::IOWait(Buf *bp)
{
	User &u = Kernel::Instance().GetUser(); // 获取当前的user结构

	/* 这里涉及到临界区
	 * 因为在执行这段程序的时候，很有可能出现硬盘中断，
	 * 在硬盘中断中，将会修改B_DONE如果此时已经进入循环
	 * 则将使得改进程永远睡眠
	 */
	X86Assembly::CLI(); // 关中断
	/*如果bp块的IO一直没完成就一直入睡*/
	while ((bp->b_flags & Buf::B_DONE) == 0)
	{
		u.u_procp->Sleep((unsigned long)bp, ProcessManager::PRIBIO); // 进程入睡，因为等待IO
	}
	X86Assembly::STI(); // 开中断

	this->GetError(bp); // 查看有没有错误发生
	return;
}

/*缓存块bp IO完成后的处理*/
void BufferManager::IODone(Buf *bp)
{
	/* 置上I/O完成标志 */
	bp->b_flags |= Buf::B_DONE;
	if (bp->b_flags & Buf::B_ASYNC)
	{
		/* 如果是异步操作,立即释放缓存块 */
		this->Brelse(bp);
	}
	else
	{
		/* 清除B_WANTED标志位，唤醒等待IO完成的进程 */
		bp->b_flags &= (~Buf::B_WANTED);
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)bp); // 唤醒等待bp的进程
	}
	return;
}

/*将磁盘 dev 上的数据块 blkno，同步读入 bp 管理的缓存*/
Buf *BufferManager::Bread(short dev, int blkno)
{
	Buf *bp;
	/* 根据设备号，字符块号申请缓存 */
	bp = this->GetBlk(dev, blkno);
	/* 如果在设备队列中找到所需缓存，即B_DONE已设置，就不需进行I/O操作 */
	if (bp->b_flags & Buf::B_DONE)
	{
		return bp;
	}
	/* 没有找到相应缓存，构成I/O读请求块 */
	bp->b_flags |= Buf::B_READ;				   // 准备读标志
	bp->b_wcount = BufferManager::BUFFER_SIZE; // 设置需要传送的字节数

	/*
	 * 将I/O请求块送入相应设备I/O请求队列，如无其它I/O请求，则将立即执行本次I/O请求；
	 * 否则等待当前I/O请求执行完毕后，由中断处理程序启动执行此请求。
	 * 注：Strategy()函数将I/O请求块送入设备请求队列后，不等I/O操作执行完毕，就直接返回。
	 */
	this->m_DeviceManager->GetBlockDevice(Utility::GetMajor(dev)).Strategy(bp);
	/* 同步读，等待I/O操作结束 */
	this->IOWait(bp);
	return bp;
}

/*预读函数，读入当前块blkno，同时异步读入预读块rablkno*/
Buf *BufferManager::Breada(short adev, int blkno, int rablkno)
{
	Buf *bp = NULL;						   /* 非预读字符块的缓存Buf */
	Buf *abp;							   /* 预读字符块的缓存Buf */
	short major = Utility::GetMajor(adev); /* 主设备号 */

	/* 当前字符块是否已在设备Buf队列中 */
	if (!this->InCore(adev, blkno))
	{
		bp = this->GetBlk(adev, blkno); /* 若没找到，GetBlk()分配缓存 */

		/* 如果分配到缓存的B_DONE标志已设置，意味着在InCore()检查之后，
		 * 其它进程碰巧读取同一字符块，因而在GetBlk()中再次搜索的时候
		 * 发现该字符块已在设备Buf队列缓冲区中，本进程重用该缓存。*/
		if ((bp->b_flags & Buf::B_DONE) == 0) // 如果已经B_DONE了，那么说明别的进程在这之前已经读入了这块字符块，不需要再读了
		{
			/* 构成读请求块 */
			bp->b_flags |= Buf::B_READ;				   // 准备读信号
			bp->b_wcount = BufferManager::BUFFER_SIZE; // 设置需要传送的字节数
			/* 驱动块设备进行I/O操作 */
			this->m_DeviceManager->GetBlockDevice(major).Strategy(bp);
		}
	}
	else
		/*UNIX V6没这条语句。加入的原因：如果当前块在缓存池中，放弃预读
		 * 这是因为，预读的价值在于利用当前块和预读块磁盘位置大概率邻近的事实，
		 * 用预读操作减少磁臂移动次数提高有效磁盘读带宽。若当前块在缓存池，磁头不一定在当前块所在的位置，
		 * 此时预读，收益有限*/
		rablkno = 0;

	/* 预读操作有2点值得注意：
	 * 1、rablkno为0，说明UNIX打算放弃预读。
	 *      这是开销与收益的权衡
	 * 2、若预读字符块在设备Buf队列中，针对预读块的操作已经成功
	 * 		这是因为：
	 * 		作为预读块，并非是进程此次读盘的目的。
	 * 		所以如果不及时释放，将使得预读块一直得不到释放。
	 * 		而将其释放它依然存在在设备队列中，如果在短时间内
	 * 		使用这一块，那么依然可以找到。
	 * */
	if (rablkno && !this->InCore(adev, rablkno)) // 如果打算预读，并且预读块没在设备队列中
	{
		abp = this->GetBlk(adev, rablkno); /* 若没找到，GetBlk()分配缓存 */

		/* 检查B_DONE标志位，理由同上。 */
		if (abp->b_flags & Buf::B_DONE)
		{
			/* 预读字符块已在缓存中，释放占用的缓存。
			 * 因为进程未必后面一定会使用预读的字符块，
			 * 也就不会去释放该缓存，有可能导致缓存资源
			 * 的长时间占用。
			 */
			this->Brelse(abp);
		}
		else
		{
			/* 异步读预读字符块 */
			abp->b_flags |= (Buf::B_READ | Buf::B_ASYNC);
			abp->b_wcount = BufferManager::BUFFER_SIZE; // 设置需要传送的字节数
			/* 驱动块设备进行I/O操作 */
			this->m_DeviceManager->GetBlockDevice(major).Strategy(abp);
		}
	}

	/* bp == NULL意味着InCore()函数检查时刻，非预读块在设备队列中，
	 * 但是InCore()只是“检查”，并不“摘取”。经过一段时间执行到此处，
	 * 有可能该字符块已经重新分配它用。
	 * 因而重新调用Bread()重读字符块，Bread()中调用GetBlk()将字符块“摘取”
	 * 过来。短时间内该字符块仍在设备队列中，所以此处Bread()一般也就是将
	 * 缓存重用，而不必重新执行一次I/O读取操作。
	 */
	if (NULL == bp)
	{
		return (this->Bread(adev, blkno)); // 同步读入<blkno,adev>，这是一个保险操作，防止刚才在设备队列中的缓存现在不在了
	}

	/* InCore()函数检查时刻未找到非预读字符块，等待I/O操作完成 */
	this->IOWait(bp);
	return bp;
}

/*写操作，函数调用过程中，进程会入睡*/
void BufferManager::Bwrite(Buf *bp)
{
	unsigned int flags;

	flags = bp->b_flags;
	bp->b_flags &= ~(Buf::B_READ | Buf::B_DONE | Buf::B_ERROR | Buf::B_DELWRI); // 清除B_READ，B_DONE，B_ERROR，B_DELWRI标记
	bp->b_wcount = BufferManager::BUFFER_SIZE;									/* 512字节 */

	this->m_DeviceManager->GetBlockDevice(Utility::GetMajor(bp->b_dev)).Strategy(bp);

	if ((flags & Buf::B_ASYNC) == 0) // 如果没有异步标志
	{
		/* 同步写，需要等待I/O操作结束 */
		this->IOWait(bp);
		this->Brelse(bp); // 写完就释放
	}
	else if ((flags & Buf::B_DELWRI) == 0)
	{
		/*
		 * 如果不是延迟写，则检查错误；否则不检查。
		 * 这是因为如果延迟写，则很有可能当前进程不是
		 * 操作这一缓存块的进程，而在GetError()主要是
		 * 给当前进程附上错误标志。
		 */
		this->GetError(bp);
	}
	return;
}

/*延迟写操作，数据写入缓存之后，不必立即送 IO 请求队列启动写操作。*/
void BufferManager::Bdwrite(Buf *bp)
{
	/* 置上B_DONE允许其它进程使用该磁盘块内容 */
	bp->b_flags |= (Buf::B_DELWRI | Buf::B_DONE);
	this->Brelse(bp); // 置完标志之后就直接释放
	return;
}
/*异步写操作，使得调用Bwrite时不用IOWait*/
void BufferManager::Bawrite(Buf *bp)
{
	/* 标记为异步写 */
	bp->b_flags |= Buf::B_ASYNC;
	this->Bwrite(bp);
	return;
}

// 将缓冲区中数据清零
void BufferManager::ClrBuf(Buf *bp)
{
	int *pInt = (int *)bp->b_addr; // 获得缓冲区的首地址

	/* 将缓冲区中数据清零 */
	for (unsigned int i = 0; i < BufferManager::BUFFER_SIZE / sizeof(int); i++)
	{
		pInt[i] = 0;
	}
	return;
}

// 把自由队列中设备号是dev的延迟写的块全异步写回去
void BufferManager::Bflush(short dev)
{
	Buf *bp;
	/* 注意：这里之所以要在搜索到一个块之后重新开始搜索，
	 * 因为在bwite()进入到驱动程序中时有开中断的操作，所以
	 * 等到bwrite执行完成后，CPU已处于开中断状态，所以很
	 * 有可能在这期间产生磁盘中断，使得bfreelist队列出现变化，
	 * 如果这里继续往下搜索，而不是重新开始搜索那么很可能在
	 * 操作bfreelist队列的时候出现错误。
	 */
loop:
	X86Assembly::CLI(); // 关中断
	// 遍历整个自由缓存队列
	for (bp = this->bFreeList.av_forw; bp != &(this->bFreeList); bp = bp->av_forw)
	{
		/* 找出自由队列中所有延迟写的块 */
		if ((bp->b_flags & Buf::B_DELWRI) && (dev == DeviceManager::NODEV || dev == bp->b_dev))
		{
			bp->b_flags |= Buf::B_ASYNC; // 置异步标志
			this->NotAvail(bp);			 // 从自由队列中取出这个块
			this->Bwrite(bp);			 // 异步写
			goto loop;
		}
	}
	X86Assembly::STI(); // 开中断
	return;
}

bool BufferManager::Swap(int blkno, unsigned long addr, int count, enum Buf::BufFlag flag)
{
	User &u = Kernel::Instance().GetUser(); // 获得当前的user结构

	X86Assembly::CLI(); // 关中断

	/* swbuf正在被其它进程使用，则睡眠等待 */
	while (this->SwBuf.b_flags & Buf::B_BUSY)
	{
		this->SwBuf.b_flags |= Buf::B_WANTED;						   // 给SwBuf置B_WANTED标志
		u.u_procp->Sleep((unsigned long)&SwBuf, ProcessManager::PSWP); // 入睡
	}

	this->SwBuf.b_flags = Buf::B_BUSY | flag;	// 开始Swap，置busy标志
	this->SwBuf.b_dev = DeviceManager::ROOTDEV; // SwBuf对应的设备是磁盘
	this->SwBuf.b_wcount = count;				// 需要传输的字节数
	this->SwBuf.b_blkno = blkno;				// 磁盘逻辑扇区号
	/* b_addr指向要传输部分的内存首地址 */
	this->SwBuf.b_addr = (unsigned char *)addr;
	this->m_DeviceManager->GetBlockDevice(Utility::GetMajor(this->SwBuf.b_dev)).Strategy(&this->SwBuf); // 放IO缓存队列

	/* 关中断进行B_DONE标志的检查 */
	X86Assembly::CLI();
	/* 这里Sleep()等同于同步I/O中IOWait()的效果 */
	while ((this->SwBuf.b_flags & Buf::B_DONE) == 0)
	{
		// 如果SwBuf还在工作，就入睡
		u.u_procp->Sleep((unsigned long)&SwBuf, ProcessManager::PSWP);
	}

	/* 这里Wakeup()等同于Brelse()的效果 */
	if (this->SwBuf.b_flags & Buf::B_WANTED)
	{
		// 唤醒等待使用SwBuf的其他进程
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)&SwBuf);
	}
	X86Assembly::STI();									   // 开中断
	this->SwBuf.b_flags &= ~(Buf::B_BUSY | Buf::B_WANTED); // 清除B_BUSY和B_WANTED标志

	if (this->SwBuf.b_flags & Buf::B_ERROR) // 检查是否出错
	{
		return false;
	}
	return true;
}

/*检查缓存块bp是否出错*/
void BufferManager::GetError(Buf *bp)
{
	User &u = Kernel::Instance().GetUser(); // 获取当前的user结构

	if (bp->b_flags & Buf::B_ERROR)
	{
		u.u_error = User::EIO; // 如果出错，给user结构也置出错标志
	}
	return;
}

/*将缓存块bp从自由缓存队列中取出来*/
void BufferManager::NotAvail(Buf *bp)
{
	X86Assembly::CLI(); /* spl6();  UNIX V6的做法 */
	/* 从自由队列中取出 */
	bp->av_back->av_forw = bp->av_forw;
	bp->av_forw->av_back = bp->av_back;
	/* 设置B_BUSY标志 */
	bp->b_flags |= Buf::B_BUSY;
	X86Assembly::STI();
	return;
}

/*检查blkno是否在adev的设备队列中*/
Buf *BufferManager::InCore(short adev, int blkno)
{
	Buf *bp;
	Devtab *dp;
	short major = Utility::GetMajor(adev); // 获取主设备号

	dp = this->m_DeviceManager->GetBlockDevice(major).d_tab; // 获取块设备表
	// 遍历整个设备队列
	for (bp = dp->b_forw; bp != (Buf *)dp; bp = bp->b_forw)
	{
		if (bp->b_blkno == blkno && bp->b_dev == adev) // 找到blkno
			return bp;
	}
	return NULL;
}

/*获取SwBuf*/
Buf &BufferManager::GetSwapBuf()
{
	return this->SwBuf;
}

/*获取自由队列队首*/
Buf &BufferManager::GetBFreeList()
{
	return this->bFreeList;
}
