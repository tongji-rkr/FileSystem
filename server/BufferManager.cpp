#include "BufferManager.h"
#include "Kernel.h"
#include<string.h>
#include<iostream>
using namespace std;
BufferManager::BufferManager()
{
	//nothing to do here
}

BufferManager::~BufferManager()
{
	//nothing to do here
}


/**
* @brief: init the Buffer Manager
* @details: 
* 	1. bound all the buffer block in this->Buffer to a BufferManager Block in this->m_Buf
* 	2. add all the BufferManager Block in to the this->bFreeList
* 	3. add B_BUSY status to all the BufferManager Block
*/
void BufferManager::Initialize()
{
	cout<<"Initalize..."<<endl;
	int i;
	Buf* bp;

	// bFreeList is a double linked list
	this->bFreeList.b_forw = this->bFreeList.b_back = &(this->bFreeList);

	for(i = 0; i < NBUF; i++)
	{
		bp = &(this->m_Buf[i]);
		// bp->b_dev = -1;
		bp->b_addr = this->Buffer[i];
		// insert this BM Block into the FreeList
		bp->b_back = &(this->bFreeList);
		bp->b_forw = this->bFreeList.b_forw;
		this->bFreeList.b_forw->b_back = bp;
		this->bFreeList.b_forw = bp;

		// init the BufferManager Block using default setting(unlocked)
		pthread_mutex_init(&bp->buf_lock, NULL);

		/* We need to lock all the buffer before change its states*/

		// set all the buffer in the free list as B_BUSY
		pthread_mutex_lock(&bp->buf_lock);
		bp->b_flags = Buf::B_BUSY;
		// unlock the block without changing its state
		Brelse(bp);

	}
	// this->m_DeviceManager = &Kernel::Instance().GetDeviceManager();
	return;
}

/**
 * @brief: assign a Buffer Manager Block to the block with blkno
 * @details:
 * 	1. if the block with blkno is already in the freeList, wait it to unlock and return it!
 * 	2. otherwise, wait the first Block Manager in the Free List to be unlock, assign it to blkno, return it
*/
Buf* BufferManager::GetBlk(int blkno){
	
	Buf*headbp=&this->bFreeList; //取得自有缓存队列的队首地址
	Buf*bp; //返回的bp 
	// traverse the bFreeList, if the wanted block is in the free list, return it directly
	for (bp = headbp->b_forw; bp != headbp; bp = bp->b_forw)
	{
		//cout<<"block_no"<<bp->b_blkno<<endl;
		if (bp->b_blkno != blkno)
			continue;
		bp->b_flags |= Buf::B_BUSY;
		// lock this buf block before return it
		pthread_mutex_lock(&bp->buf_lock);
		//cout << "在缓存队列中找到对应的缓存，置为busy，GetBlk返回 blkno=" <<blkno<< endl;
		return bp;
	}

	// the wanted block is not in the freelist, try to assign an unlocked ManagerBlock to it
	int success = false;
	for (bp = headbp->b_forw; bp != headbp; bp = bp->b_forw)
	{
		// if a unlocked block is found
		if(pthread_mutex_trylock(&bp->buf_lock)==0){
			success = true;
			break;
		}
		printf("[DEBUG] The buf is locked, blkno=%d b_addr=%p\n", bp->b_blkno, bp->b_addr);
	}

	// all the block in the free list is already locked
	if(success == false){
		bp = headbp->b_forw;
		printf("[INFO]System Buffer ran out, wait the first block unlock...\n");
		// wait for the first buffer block in the free buffer list to unlock
		pthread_mutex_lock(&bp->buf_lock); 
		printf("[INFO]Succesfully get a Buffer Block..\n");
	}
	// if the content in this blokc is not synced with the disk(B_DELWRI), write it back!
	if (bp->b_flags&Buf::B_DELWRI)
	{
		this->Bwrite(bp);
		// lock it immediately after write it back!
		pthread_mutex_lock(&bp->buf_lock);
	}
	// clean all the states of the selected block except the B_BUSY
	bp->b_flags = Buf::B_BUSY;
	bp->b_blkno = blkno;
	return bp;
}

/**
 * @brief: unlock a buffer block without changing its B_WANTED B_BUSY B_WANTED state
*/
void BufferManager::Brelse(Buf* bp)
{
	/* 注意以下操作并没有清除B_DELWRI、B_WRITE、B_READ、B_DONE标志
	 * B_DELWRI表示虽然将该控制块释放到自由队列里面，但是有可能还没有些到磁盘上。
	 * B_DONE则是指该缓存的内容正确地反映了存储在或应存储在磁盘上的信息 
	 */
	bp->b_flags &= ~(Buf::B_WANTED | Buf::B_BUSY | Buf::B_ASYNC);
	pthread_mutex_unlock(&bp->buf_lock);
	//printf("[DEBUG] 释放缓存块 b")
	return;
}

/**
 * @brief read the buffer block
 * 
 * @param blkno 
 * @return Buf* 
 */
Buf* BufferManager::Bread(int blkno)
{
	Buf* bp;
	// alloc a buffer manager block for the block with blkno
	// blkno is the block number in the disk
	bp = this->GetBlk(blkno);
	// if this block is already in the buffer, and the content is synced with the disk, return it directly
	if(bp->b_flags & Buf::B_DONE)
	{
		return bp;
	}
	/* 没有找到相应缓存，构成I/O读请求块 */
	bp->b_flags |= Buf::B_READ;
	bp->b_wcount = BufferManager::BUFFER_SIZE;
	// 拷贝到内存
	memcpy(bp->b_addr,&this->p[BufferManager::BUFFER_SIZE*bp->b_blkno],BufferManager::BUFFER_SIZE);

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
	cout<<"Bflush"<<endl;
	Buf* bp;
	/* 注意：这里之所以要在搜索到一个块之后重新开始搜索，
	 * 因为在bwite()进入到驱动程序中时有开中断的操作，所以
	 * 等到bwrite执行完成后，CPU已处于开中断状态，所以很
	 * 有可能在这期间产生磁盘中断，使得bfreelist队列出现变化，
	 * 如果这里继续往下搜索，而不是重新开始搜索那么很可能在
	 * 操作bfreelist队列的时候出现错误。
	 */
// loop:
//	X86Assembly::CLI();
	for(bp = this->bFreeList.b_forw; bp != &(this->bFreeList); bp = bp->b_forw)
	{
		/* 找出自由队列中所有延迟写的块 */
		if( (bp->b_flags & Buf::B_DELWRI)) //&& (dev == DeviceManager::NODEV || dev == bp->b_dev) )
		{
			// 把当前的buf从队列里拿出来（修改前面和后面buf的指针
			bp->b_back->b_forw = bp->b_forw;
			bp->b_forw->b_back = bp->b_back;
			// buf后向指针指向头的前一个？？？为啥
			bp->b_back = this->bFreeList.b_back->b_forw;
			// 头的后一个buf的前一个指向buf
			this->bFreeList.b_back->b_forw = bp;
			// buf的前向指向头
			bp->b_forw = &this->bFreeList;
			// 头的后向是buf
			this->bFreeList.b_back = bp;
			// 我们这里没有异步
			// bp->b_flags |= Buf::B_ASYNC;
			// this->NotAvail(bp);
			this->Bwrite(bp);
			// goto loop;
		}
	}
	// X86Assembly::STI();
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

// void BufferManager::NotAvail(Buf *bp)
// {
// 	X86Assembly::CLI();		/* spl6();  UNIX V6的做法 */
// 	/* 从自由队列中取出 */
// 	bp->av_back->av_forw = bp->av_forw;
// 	bp->av_forw->av_back = bp->av_back;
// 	/* 设置B_BUSY标志 */
// 	bp->b_flags |= Buf::B_BUSY;
// 	X86Assembly::STI();
// 	return;
// }

Buf* BufferManager::InCore( int blkno)
{
	cout<<"Incore"<<endl;
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

// Buf& BufferManager::GetSwapBuf()
// {
// 	return this->SwBuf;
// }

Buf& BufferManager::GetBFreeList()
{
	return this->bFreeList;
}

