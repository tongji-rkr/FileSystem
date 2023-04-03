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
// ��ʼ��
void BufferManager::Initialize()
{
	int i;
	Buf *bp;

	// ����Ŀǰ�������ж��ǿյģ����������ɻ�����ƿ���й���ָ�붼ָ��������
	this->bFreeList.b_forw = this->bFreeList.b_back = &(this->bFreeList);
	this->bFreeList.av_forw = this->bFreeList.av_back = &(this->bFreeList);

	for (i = 0; i < NBUF; i++)
	{
		bp = &(this->m_Buf[i]);
		bp->b_dev = -1;				  // ���豸�źʹ��豸�ų�ʼ��Ϊ����λ����1���
		bp->b_addr = this->Buffer[i]; // ָ��������Ļ��������׵�ַ
		/* ��ʼ��NODEV���� */
		bp->b_back = &(this->bFreeList);	 // ָ�����ɶ��еĶ�β
		bp->b_forw = this->bFreeList.b_forw; // ָ�����ɶ��еĶ���
		this->bFreeList.b_forw->b_back = bp; // ���ɶ��еĶ��׵�ǰ��ָ��bp
		this->bFreeList.b_forw = bp;		 // ���ɶ��еĶ���ָ��bp
		/* ��ʼ�����ɶ��� */
		bp->b_flags = Buf::B_BUSY; // ��æ��־
		Brelse(bp);				   // ����
	}
	// ָ���豸����ģ��ȫ�ֶ���
	this->m_DeviceManager = &Kernel::Instance().GetDeviceManager();
	return;
}

// ���������������ס������У�����װ<dev,blkno>�Ļ����
Buf *BufferManager::GetBlk(short dev, int blkno)
{
	Buf *bp;
	Devtab *dp;
	User &u = Kernel::Instance().GetUser(); // ��ȡ��ǰ��user�ṹ

	/* ������豸�ų�����ϵͳ�п��豸���� */
	if (Utility::GetMajor(dev) >= this->m_DeviceManager->GetNBlkDev())
	{
		Utility::Panic("nblkdev: There doesn't exist the device"); // ���������Ϣ
	}

	/*
	 * ����豸�������Ѿ�������Ӧ���棬�򷵻ظû��棻
	 * ��������ɶ����з����µĻ��������ַ����д��
	 */
loop:
	/* ��ʾ����NODEV�豸���ַ��� */
	if (dev < 0)
	{
		dp = (Devtab *)(&this->bFreeList);
	}
	else
	{
		short major = Utility::GetMajor(dev); // ���豸��
		/* �������豸�Ż�ÿ��豸�� */
		dp = this->m_DeviceManager->GetBlockDevice(major).d_tab;

		// ���û�п��豸������Ҫ����
		if (dp == NULL)
		{
			Utility::Panic("Null devtab!");
		}
		/* �����ڸ��豸�����������Ƿ�����Ӧ�Ļ��� */
		for (bp = dp->b_forw; bp != (Buf *)dp; bp = bp->b_forw)
		{
			/* ����Ҫ�ҵĻ��棬����� */
			if (bp->b_blkno != blkno || bp->b_dev != dev)
				continue;

			/*
			 * �ٽ���֮����Ҫ�����￪ʼ�������Ǵ������forѭ����ʼ��
			 * ��Ҫ����Ϊ���жϷ�����򲢲���ȥ�޸Ŀ��豸���е�
			 * �豸buf����(b_forw)�����Բ��������ͻ��
			 */
			X86Assembly::CLI();			   // ���ж�
			if (bp->b_flags & Buf::B_BUSY) // ���Ŀǰ�û�����Ѿ���ʹ����
			{
				bp->b_flags |= Buf::B_WANTED;								 // �û���鱻�ȴ�ʹ��
				u.u_procp->Sleep((unsigned long)bp, ProcessManager::PRIBIO); // ������˯
				X86Assembly::STI();											 // ���ж�
				goto loop;													 // ����ѭ��
			}
			X86Assembly::STI(); // ���ж�
			/* �����ɶ����г�ȡ���� */
			this->NotAvail(bp);
			return bp;
		}
	} // end of else
	/*������е�����˵���ڸ��豸������û���ҵ���Ӧ�Ļ��棬
	 * �����治���У���ô����Ҫ��һ�����ɻ�������еĻ���������������
	 * */

	X86Assembly::CLI();
	/* ������ɶ���Ϊ�� */
	if (this->bFreeList.av_forw == &this->bFreeList)
	{
		this->bFreeList.b_flags |= Buf::B_WANTED;								   // �õȴ���־
		u.u_procp->Sleep((unsigned long)&this->bFreeList, ProcessManager::PRIBIO); // ��˯
		X86Assembly::STI();														   // ���ж�
		goto loop;																   // ��˯ѭ���ȴ�
	}
	X86Assembly::STI(); // ���ж�

	/* ȡ���ɶ��е�һ�����п� */
	bp = this->bFreeList.av_forw;
	this->NotAvail(bp);

	/* ������ַ������ӳ�д�������첽д�������� */
	if (bp->b_flags & Buf::B_DELWRI)
	{
		bp->b_flags |= Buf::B_ASYNC; // ���첽��־
		this->Bwrite(bp);			 // �첽д��ȥ
		goto loop;					 // ѭ��
	}
	/* ע��: �����������������λ��ֻ����B_BUSY */
	bp->b_flags = Buf::B_BUSY;

	/* ��ԭ�豸�����г�� */
	bp->b_back->b_forw = bp->b_forw; // ԭ�������и���ǰһ��ĺ�һ���ɸ���ĺ�һ��
	bp->b_forw->b_back = bp->b_back; // ԭ�������и����һ���ǰһ���ɸ����ǰһ��
	/* �����µ��豸���� */
	bp->b_forw = dp->b_forw;
	bp->b_back = (Buf *)dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;

	bp->b_dev = dev;	 // �޸ĸû������豸��
	bp->b_blkno = blkno; // �޸ĸû����Ŀ��
	return bp;
}

/*����ʹ����ϣ������ͷ�*/
void BufferManager::Brelse(Buf *bp)
{
	ProcessManager &procMgr = Kernel::Instance().GetProcessManager(); // ��ȡ��ǰ�Ľ��̹�����

	// ��������bp���ڱ��ȴ�ʹ�ã����ѵȴ��û����Ľ���
	if (bp->b_flags & Buf::B_WANTED)
	{
		procMgr.WakeUpAll((unsigned long)bp);
	}

	/* ����н������ڵȴ��������ɶ����еĻ��棬������Ӧ���̣���Ϊ�ͷ�������������̣����ͳ����ɵ��ˣ� */
	if (this->bFreeList.b_flags & Buf::B_WANTED)
	{
		/* ���B_WANTED��־����ʾ���п��л��� */
		this->bFreeList.b_flags &= (~Buf::B_WANTED);
		procMgr.WakeUpAll((unsigned long)&this->bFreeList); // ���ѵȴ����ɻ���Ľ���
	}

	/* ����д��������򽫴��豸�Ÿĵ���
	 * ��������������û������еĴ������ݡ�
	 */
	if (bp->b_flags & Buf::B_ERROR)
	{
		Utility::SetMinor(bp->b_dev, -1);
	}

	/* �ٽ���Դ�����磺��ͬ����ĩ�ڻ�������������
	 * ��ʱ���п��ܻ���������жϣ�ͬ����������������
	 */
	X86Assembly::CLI(); /* spl6();  UNIX V6������ */

	/* ע�����²�����û�����B_DELWRI��B_WRITE��B_READ��B_DONE��־
	 * B_DELWRI��ʾ��Ȼ���ÿ��ƿ��ͷŵ����ɶ������棬�����п��ܻ�û��Щ�������ϡ�
	 * B_DONE����ָ�û����������ȷ�ط�ӳ�˴洢�ڻ�Ӧ�洢�ڴ����ϵ���Ϣ
	 */
	bp->b_flags &= ~(Buf::B_WANTED | Buf::B_BUSY | Buf::B_ASYNC); // ���B_WANTED��B_BUSY��B_ASYNC
	(this->bFreeList.av_back)->av_forw = bp;					  // ��bp�嵽���ɻ�����еĶ�β
	bp->av_back = this->bFreeList.av_back;						  // bp��ǰһ�������ͳ�ԭ�����ɻ�����ж�β�Ļ����
	bp->av_forw = &(this->bFreeList);							  // bp����û�б�Ļ������
	this->bFreeList.av_back = bp;								  // bp��Ϊ�����ɻ�����еĶ�β

	X86Assembly::STI(); // ���ж�
	return;
}

/*������˯�ȵȴ�IO���*/
void BufferManager::IOWait(Buf *bp)
{
	User &u = Kernel::Instance().GetUser(); // ��ȡ��ǰ��user�ṹ

	/* �����漰���ٽ���
	 * ��Ϊ��ִ����γ����ʱ�򣬺��п��ܳ���Ӳ���жϣ�
	 * ��Ӳ���ж��У������޸�B_DONE�����ʱ�Ѿ�����ѭ��
	 * ��ʹ�øĽ�����Զ˯��
	 */
	X86Assembly::CLI(); // ���ж�
	/*���bp���IOһֱû��ɾ�һֱ��˯*/
	while ((bp->b_flags & Buf::B_DONE) == 0)
	{
		u.u_procp->Sleep((unsigned long)bp, ProcessManager::PRIBIO); // ������˯����Ϊ�ȴ�IO
	}
	X86Assembly::STI(); // ���ж�

	this->GetError(bp); // �鿴��û�д�����
	return;
}

/*�����bp IO��ɺ�Ĵ���*/
void BufferManager::IODone(Buf *bp)
{
	/* ����I/O��ɱ�־ */
	bp->b_flags |= Buf::B_DONE;
	if (bp->b_flags & Buf::B_ASYNC)
	{
		/* ������첽����,�����ͷŻ���� */
		this->Brelse(bp);
	}
	else
	{
		/* ���B_WANTED��־λ�����ѵȴ�IO��ɵĽ��� */
		bp->b_flags &= (~Buf::B_WANTED);
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)bp); // ���ѵȴ�bp�Ľ���
	}
	return;
}

/*������ dev �ϵ����ݿ� blkno��ͬ������ bp ����Ļ���*/
Buf *BufferManager::Bread(short dev, int blkno)
{
	Buf *bp;
	/* �����豸�ţ��ַ�������뻺�� */
	bp = this->GetBlk(dev, blkno);
	/* ������豸�������ҵ����軺�棬��B_DONE�����ã��Ͳ������I/O���� */
	if (bp->b_flags & Buf::B_DONE)
	{
		return bp;
	}
	/* û���ҵ���Ӧ���棬����I/O������� */
	bp->b_flags |= Buf::B_READ;				   // ׼������־
	bp->b_wcount = BufferManager::BUFFER_SIZE; // ������Ҫ���͵��ֽ���

	/*
	 * ��I/O�����������Ӧ�豸I/O������У���������I/O����������ִ�б���I/O����
	 * ����ȴ���ǰI/O����ִ����Ϻ����жϴ����������ִ�д�����
	 * ע��Strategy()������I/O����������豸������к󣬲���I/O����ִ����ϣ���ֱ�ӷ��ء�
	 */
	this->m_DeviceManager->GetBlockDevice(Utility::GetMajor(dev)).Strategy(bp);
	/* ͬ�������ȴ�I/O�������� */
	this->IOWait(bp);
	return bp;
}

/*Ԥ�����������뵱ǰ��blkno��ͬʱ�첽����Ԥ����rablkno*/
Buf *BufferManager::Breada(short adev, int blkno, int rablkno)
{
	Buf *bp = NULL;						   /* ��Ԥ���ַ���Ļ���Buf */
	Buf *abp;							   /* Ԥ���ַ���Ļ���Buf */
	short major = Utility::GetMajor(adev); /* ���豸�� */

	/* ��ǰ�ַ����Ƿ������豸Buf������ */
	if (!this->InCore(adev, blkno))
	{
		bp = this->GetBlk(adev, blkno); /* ��û�ҵ���GetBlk()���仺�� */

		/* ������䵽�����B_DONE��־�����ã���ζ����InCore()���֮��
		 * �����������ɶ�ȡͬһ�ַ��飬�����GetBlk()���ٴ�������ʱ��
		 * ���ָ��ַ��������豸Buf���л������У����������øû��档*/
		if ((bp->b_flags & Buf::B_DONE) == 0) // ����Ѿ�B_DONE�ˣ���ô˵����Ľ�������֮ǰ�Ѿ�����������ַ��飬����Ҫ�ٶ���
		{
			/* ���ɶ������ */
			bp->b_flags |= Buf::B_READ;				   // ׼�����ź�
			bp->b_wcount = BufferManager::BUFFER_SIZE; // ������Ҫ���͵��ֽ���
			/* �������豸����I/O���� */
			this->m_DeviceManager->GetBlockDevice(major).Strategy(bp);
		}
	}
	else
		/*UNIX V6û������䡣�����ԭ�������ǰ���ڻ�����У�����Ԥ��
		 * ������Ϊ��Ԥ���ļ�ֵ�������õ�ǰ���Ԥ�������λ�ô�����ڽ�����ʵ��
		 * ��Ԥ���������ٴű��ƶ����������Ч���̶���������ǰ���ڻ���أ���ͷ��һ���ڵ�ǰ�����ڵ�λ�ã�
		 * ��ʱԤ������������*/
		rablkno = 0;

	/* Ԥ��������2��ֵ��ע�⣺
	 * 1��rablknoΪ0��˵��UNIX�������Ԥ����
	 *      ���ǿ����������Ȩ��
	 * 2����Ԥ���ַ������豸Buf�����У����Ԥ����Ĳ����Ѿ��ɹ�
	 * 		������Ϊ��
	 * 		��ΪԤ���飬�����ǽ��̴˴ζ��̵�Ŀ�ġ�
	 * 		�����������ʱ�ͷţ���ʹ��Ԥ����һֱ�ò����ͷš�
	 * 		�������ͷ�����Ȼ�������豸�����У�����ڶ�ʱ����
	 * 		ʹ����һ�飬��ô��Ȼ�����ҵ���
	 * */
	if (rablkno && !this->InCore(adev, rablkno)) // �������Ԥ��������Ԥ����û���豸������
	{
		abp = this->GetBlk(adev, rablkno); /* ��û�ҵ���GetBlk()���仺�� */

		/* ���B_DONE��־λ������ͬ�ϡ� */
		if (abp->b_flags & Buf::B_DONE)
		{
			/* Ԥ���ַ������ڻ����У��ͷ�ռ�õĻ��档
			 * ��Ϊ����δ�غ���һ����ʹ��Ԥ�����ַ��飬
			 * Ҳ�Ͳ���ȥ�ͷŸû��棬�п��ܵ��»�����Դ
			 * �ĳ�ʱ��ռ�á�
			 */
			this->Brelse(abp);
		}
		else
		{
			/* �첽��Ԥ���ַ��� */
			abp->b_flags |= (Buf::B_READ | Buf::B_ASYNC);
			abp->b_wcount = BufferManager::BUFFER_SIZE; // ������Ҫ���͵��ֽ���
			/* �������豸����I/O���� */
			this->m_DeviceManager->GetBlockDevice(major).Strategy(abp);
		}
	}

	/* bp == NULL��ζ��InCore()�������ʱ�̣���Ԥ�������豸�����У�
	 * ����InCore()ֻ�ǡ���顱��������ժȡ��������һ��ʱ��ִ�е��˴���
	 * �п��ܸ��ַ����Ѿ����·������á�
	 * ������µ���Bread()�ض��ַ��飬Bread()�е���GetBlk()���ַ��顰ժȡ��
	 * ��������ʱ���ڸ��ַ��������豸�����У����Դ˴�Bread()һ��Ҳ���ǽ�
	 * �������ã�����������ִ��һ��I/O��ȡ������
	 */
	if (NULL == bp)
	{
		return (this->Bread(adev, blkno)); // ͬ������<blkno,adev>������һ�����ղ�������ֹ�ղ����豸�����еĻ������ڲ�����
	}

	/* InCore()�������ʱ��δ�ҵ���Ԥ���ַ��飬�ȴ�I/O������� */
	this->IOWait(bp);
	return bp;
}

/*д�������������ù����У����̻���˯*/
void BufferManager::Bwrite(Buf *bp)
{
	unsigned int flags;

	flags = bp->b_flags;
	bp->b_flags &= ~(Buf::B_READ | Buf::B_DONE | Buf::B_ERROR | Buf::B_DELWRI); // ���B_READ��B_DONE��B_ERROR��B_DELWRI���
	bp->b_wcount = BufferManager::BUFFER_SIZE;									/* 512�ֽ� */

	this->m_DeviceManager->GetBlockDevice(Utility::GetMajor(bp->b_dev)).Strategy(bp);

	if ((flags & Buf::B_ASYNC) == 0) // ���û���첽��־
	{
		/* ͬ��д����Ҫ�ȴ�I/O�������� */
		this->IOWait(bp);
		this->Brelse(bp); // д����ͷ�
	}
	else if ((flags & Buf::B_DELWRI) == 0)
	{
		/*
		 * ��������ӳ�д��������󣻷��򲻼�顣
		 * ������Ϊ����ӳ�д������п��ܵ�ǰ���̲���
		 * ������һ�����Ľ��̣�����GetError()��Ҫ��
		 * ����ǰ���̸��ϴ����־��
		 */
		this->GetError(bp);
	}
	return;
}

/*�ӳ�д����������д�뻺��֮�󣬲��������� IO �����������д������*/
void BufferManager::Bdwrite(Buf *bp)
{
	/* ����B_DONE������������ʹ�øô��̿����� */
	bp->b_flags |= (Buf::B_DELWRI | Buf::B_DONE);
	this->Brelse(bp); // �����־֮���ֱ���ͷ�
	return;
}
/*�첽д������ʹ�õ���Bwriteʱ����IOWait*/
void BufferManager::Bawrite(Buf *bp)
{
	/* ���Ϊ�첽д */
	bp->b_flags |= Buf::B_ASYNC;
	this->Bwrite(bp);
	return;
}

// ������������������
void BufferManager::ClrBuf(Buf *bp)
{
	int *pInt = (int *)bp->b_addr; // ��û��������׵�ַ

	/* ������������������ */
	for (unsigned int i = 0; i < BufferManager::BUFFER_SIZE / sizeof(int); i++)
	{
		pInt[i] = 0;
	}
	return;
}

// �����ɶ������豸����dev���ӳ�д�Ŀ�ȫ�첽д��ȥ
void BufferManager::Bflush(short dev)
{
	Buf *bp;
	/* ע�⣺����֮����Ҫ��������һ����֮�����¿�ʼ������
	 * ��Ϊ��bwite()���뵽����������ʱ�п��жϵĲ���������
	 * �ȵ�bwriteִ����ɺ�CPU�Ѵ��ڿ��ж�״̬�����Ժ�
	 * �п��������ڼ���������жϣ�ʹ��bfreelist���г��ֱ仯��
	 * �����������������������������¿�ʼ������ô�ܿ�����
	 * ����bfreelist���е�ʱ����ִ���
	 */
loop:
	X86Assembly::CLI(); // ���ж�
	// �����������ɻ������
	for (bp = this->bFreeList.av_forw; bp != &(this->bFreeList); bp = bp->av_forw)
	{
		/* �ҳ����ɶ����������ӳ�д�Ŀ� */
		if ((bp->b_flags & Buf::B_DELWRI) && (dev == DeviceManager::NODEV || dev == bp->b_dev))
		{
			bp->b_flags |= Buf::B_ASYNC; // ���첽��־
			this->NotAvail(bp);			 // �����ɶ�����ȡ�������
			this->Bwrite(bp);			 // �첽д
			goto loop;
		}
	}
	X86Assembly::STI(); // ���ж�
	return;
}

bool BufferManager::Swap(int blkno, unsigned long addr, int count, enum Buf::BufFlag flag)
{
	User &u = Kernel::Instance().GetUser(); // ��õ�ǰ��user�ṹ

	X86Assembly::CLI(); // ���ж�

	/* swbuf���ڱ���������ʹ�ã���˯�ߵȴ� */
	while (this->SwBuf.b_flags & Buf::B_BUSY)
	{
		this->SwBuf.b_flags |= Buf::B_WANTED;						   // ��SwBuf��B_WANTED��־
		u.u_procp->Sleep((unsigned long)&SwBuf, ProcessManager::PSWP); // ��˯
	}

	this->SwBuf.b_flags = Buf::B_BUSY | flag;	// ��ʼSwap����busy��־
	this->SwBuf.b_dev = DeviceManager::ROOTDEV; // SwBuf��Ӧ���豸�Ǵ���
	this->SwBuf.b_wcount = count;				// ��Ҫ������ֽ���
	this->SwBuf.b_blkno = blkno;				// �����߼�������
	/* b_addrָ��Ҫ���䲿�ֵ��ڴ��׵�ַ */
	this->SwBuf.b_addr = (unsigned char *)addr;
	this->m_DeviceManager->GetBlockDevice(Utility::GetMajor(this->SwBuf.b_dev)).Strategy(&this->SwBuf); // ��IO�������

	/* ���жϽ���B_DONE��־�ļ�� */
	X86Assembly::CLI();
	/* ����Sleep()��ͬ��ͬ��I/O��IOWait()��Ч�� */
	while ((this->SwBuf.b_flags & Buf::B_DONE) == 0)
	{
		// ���SwBuf���ڹ���������˯
		u.u_procp->Sleep((unsigned long)&SwBuf, ProcessManager::PSWP);
	}

	/* ����Wakeup()��ͬ��Brelse()��Ч�� */
	if (this->SwBuf.b_flags & Buf::B_WANTED)
	{
		// ���ѵȴ�ʹ��SwBuf����������
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)&SwBuf);
	}
	X86Assembly::STI();									   // ���ж�
	this->SwBuf.b_flags &= ~(Buf::B_BUSY | Buf::B_WANTED); // ���B_BUSY��B_WANTED��־

	if (this->SwBuf.b_flags & Buf::B_ERROR) // ����Ƿ����
	{
		return false;
	}
	return true;
}

/*��黺���bp�Ƿ����*/
void BufferManager::GetError(Buf *bp)
{
	User &u = Kernel::Instance().GetUser(); // ��ȡ��ǰ��user�ṹ

	if (bp->b_flags & Buf::B_ERROR)
	{
		u.u_error = User::EIO; // ���������user�ṹҲ�ó����־
	}
	return;
}

/*�������bp�����ɻ��������ȡ����*/
void BufferManager::NotAvail(Buf *bp)
{
	X86Assembly::CLI(); /* spl6();  UNIX V6������ */
	/* �����ɶ�����ȡ�� */
	bp->av_back->av_forw = bp->av_forw;
	bp->av_forw->av_back = bp->av_back;
	/* ����B_BUSY��־ */
	bp->b_flags |= Buf::B_BUSY;
	X86Assembly::STI();
	return;
}

/*���blkno�Ƿ���adev���豸������*/
Buf *BufferManager::InCore(short adev, int blkno)
{
	Buf *bp;
	Devtab *dp;
	short major = Utility::GetMajor(adev); // ��ȡ���豸��

	dp = this->m_DeviceManager->GetBlockDevice(major).d_tab; // ��ȡ���豸��
	// ���������豸����
	for (bp = dp->b_forw; bp != (Buf *)dp; bp = bp->b_forw)
	{
		if (bp->b_blkno == blkno && bp->b_dev == adev) // �ҵ�blkno
			return bp;
	}
	return NULL;
}

/*��ȡSwBuf*/
Buf &BufferManager::GetSwapBuf()
{
	return this->SwBuf;
}

/*��ȡ���ɶ��ж���*/
Buf &BufferManager::GetBFreeList()
{
	return this->bFreeList;
}
