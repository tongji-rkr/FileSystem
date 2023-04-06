#include "BufferManager.h"
#include "Kernel.h"
#include <string.h>
#include <iostream>
using namespace std;
BufferManager::BufferManager()
{
	//nothing to do here
}

BufferManager::~BufferManager()
{
	//nothing to do here
}

void BufferManager::Initialize()
{
	int i;
	Buf* bp;

	this->bFreeList.b_forw = this->bFreeList.b_back = &(this->bFreeList);
	// this->bFreeList.av_forw = this->bFreeList.av_back = &(this->bFreeList);

	for(i = 0; i < NBUF; i++)
	{
		bp = &(this->m_Buf[i]);//取得缓存块
		pthread_mutex_init(&bp->buf_lock, NULL);//初始化锁
		pthread_mutex_lock(&bp->buf_lock);//上锁
		bp->b_addr = this->Buffer[i];//取得缓存块的地址
		/* 初始化NODEV队列 */
		bp->b_back = &(this->bFreeList);//指向自有缓存队列的队尾
		bp->b_forw = this->bFreeList.b_forw;//指向自有缓存队列的队首
		this->bFreeList.b_forw->b_back = bp;//自有缓存队列的队首的前驱指向bp
		this->bFreeList.b_forw = bp;//自有缓存队列的队首指向bp
		bp->b_flags = Buf::B_BUSY;//置为busy
		Brelse(bp);//释放缓存

	}
	// this->m_DeviceManager = &Kernel::Instance().GetDeviceManager();
	return;
}


Buf* BufferManager::GetBlk(int blkno)
{
	Buf* headbp = &this->bFreeList; //取得自有缓存队列的队首地址
	Buf* bp; //返回的bp 
	// 查看bFreeList中是否已经有该块的缓存, 有就返回
	for (bp = headbp->b_forw; bp != headbp; bp = bp->b_forw)
	{
		if (bp->b_blkno != blkno)
			continue;
		pthread_mutex_lock(&bp->buf_lock);//上锁
		bp->b_flags |= Buf::B_BUSY;
		return bp;
	}

	int flag = 1;
	for (bp = headbp->b_forw; bp != headbp; bp = bp->b_forw)
	{
		if(pthread_mutex_trylock(&bp->buf_lock)==0)//尝试开锁
		{
			flag = 0;
			break;
		}
	}
	if(flag)
	{
		pthread_mutex_lock(&bp->buf_lock); // 上锁，等待第一个缓存块解锁。
		bp = headbp->b_forw;
	}
	if (bp->b_flags&Buf::B_DELWRI)
	{
		this->Bwrite(bp);                  // 写回磁盘，并解锁
		pthread_mutex_lock(&bp->buf_lock); // 上锁
	}
	//注：这里清空了其他所有的标志，只置为busy
	bp->b_flags = Buf::B_BUSY;
	bp->b_blkno = blkno;
	return bp;
}


void BufferManager::Brelse(Buf* bp)
{
	/* 注意以下操作并没有清除B_DELWRI、B_WRITE、B_READ、B_DONE标志
	 * B_DELWRI表示虽然将该控制块释放到自由队列里面，但是有可能还没有些到磁盘上。
	 * B_DONE则是指该缓存的内容正确地反映了存储在或应存储在磁盘上的信息 
	 */
	bp->b_flags &= ~(Buf::B_WANTED | Buf::B_BUSY | Buf::B_ASYNC);
	pthread_mutex_unlock(&bp->buf_lock); //缓存块释放了，解锁
	return;
}


Buf* BufferManager::Bread(int blkno)
{
	Buf* bp;
	/* 字符块号申请缓存 */
	bp = this->GetBlk(blkno);
	/* 如果在设备队列中找到所需缓存，即B_DONE已设置，就不需进行I/O操作 */
	if(bp->b_flags & Buf::B_DONE)
	{
		return bp;
	}
	/* 没有找到相应缓存，构成I/O读请求块 */
	bp->b_flags |= Buf::B_READ;
	bp->b_wcount = BufferManager::BUFFER_SIZE;
	// 拷贝到内存
	memcpy(bp->b_addr,&this->p[BufferManager::BUFFER_SIZE*bp->b_blkno],BufferManager::BUFFER_SIZE);
	/* 
	 * 将I/O请求块送入相应设备I/O请求队列，如无其它I/O请求，则将立即执行本次I/O请求；
	 * 否则等待当前I/O请求执行完毕后，由中断处理程序启动执行此请求。
	 * 注：Strategy()函数将I/O请求块送入设备请求队列后，不等I/O操作执行完毕，就直接返回。
	 */
	// this->m_DeviceManager->GetBlockDevice(Utility::GetMajor(dev)).Strategy(bp);
	/* 同步读，等待I/O操作结束 */
	// this->IOWait(bp);
	return bp;
}

void BufferManager::Bwrite(Buf *bp)
{
	bp->b_flags &= ~(Buf::B_READ | Buf::B_DONE | Buf::B_ERROR | Buf::B_DELWRI);
	bp->b_wcount = BufferManager::BUFFER_SIZE;		/* 512字节 */

	memcpy(&this->p[BufferManager::BUFFER_SIZE * bp->b_blkno], bp->b_addr, BufferManager::BUFFER_SIZE);
	this->Brelse(bp);

	return;
}

void BufferManager::Bdwrite(Buf *bp)
{
	/* 置上B_DONE允许其它进程使用该磁盘块内容 */
	bp->b_flags |= (Buf::B_DELWRI | Buf::B_DONE);
	this->Brelse(bp);
	return;
}

void BufferManager::Bawrite(Buf *bp)
{
	/* 标记为异步写 */
	bp->b_flags |= Buf::B_ASYNC;
	this->Bwrite(bp);
	return;
}

void BufferManager::ClrBuf(Buf *bp)
{
	int* pInt = (int *)bp->b_addr;

	/* 将缓冲区中数据清零 */
	for(unsigned int i = 0; i < BufferManager::BUFFER_SIZE / sizeof(int); i++)
	{
		pInt[i] = 0;
	}
	return;
}
/* 队列中延迟写的缓存全部输出到磁盘 */
void BufferManager::Bflush()
{
	Buf* bp;
	/* 注意：这里之所以要在搜索到一个块之后重新开始搜索，
	 * 因为在bwite()进入到驱动程序中时有开中断的操作，所以
	 * 等到bwrite执行完成后，CPU已处于开中断状态，所以很
	 * 有可能在这期间产生磁盘中断，使得bfreelist队列出现变化，
	 * 如果这里继续往下搜索，而不是重新开始搜索那么很可能在
	 * 操作bfreelist队列的时候出现错误。
	 */
	for(bp = this->bFreeList.b_forw; bp != &(this->bFreeList); bp = bp->b_forw)
	{
		/* 找出自由队列中所有延迟写的块 */
		if( (bp->b_flags & Buf::B_DELWRI)) //&& (dev == DeviceManager::NODEV || dev == bp->b_dev) )
		{
			// 把当前的buf从队列里拿出来（修改前面和后面buf的指针
			bp->b_back->b_forw = bp->b_forw;
			bp->b_forw->b_back = bp->b_back;
			bp->b_back = this->bFreeList.b_back->b_forw;
			this->bFreeList.b_back->b_forw = bp;
			bp->b_forw = &this->bFreeList;
			this->bFreeList.b_back = bp;
			// bp->b_flags |= Buf::B_ASYNC;
			// this->NotAvail(bp);
			this->Bwrite(bp);
		}
	}
	return;
}

void BufferManager::GetError(Buf* bp)
{
	User& u = Kernel::Instance().GetUser();

	if (bp->b_flags & Buf::B_ERROR)
	{
		u.u_error = EIO;
	}
	return;
}

Buf* BufferManager::InCore( int blkno)
{
	Buf* bp;
	// Devtab* dp;
	// short major = Utility::GetMajor(adev);
	Buf*dp=  &this->bFreeList;
	// dp = this->m_DeviceManager->GetBlockDevice(major).d_tab;
	for(bp = dp->b_forw; bp != (Buf *)dp; bp = bp->b_forw)
	{
		if(bp->b_blkno == blkno)// && bp->b_dev == adev)
			return bp;
	}
	return NULL;
}

Buf& BufferManager::GetBFreeList()
{
	return this->bFreeList;
}

void BufferManager::SetP(char* mmapaddr)
{
	this->p = mmapaddr;
}

char* BufferManager::GetP()
{
	return this->p;
}
