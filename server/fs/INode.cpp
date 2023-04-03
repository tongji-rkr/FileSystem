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
/*	Ԥ����Ŀ�ţ�����ͨ�ļ�����Ԥ�������ڵ������š���Ӳ�̶��ԣ����ǵ�ǰ����飨����������һ������飨������*/
int Inode::rablock = 0;

/* �ڴ�� i�ڵ�*/
Inode::Inode()
{
	/* ���Inode�����е����� */
	// this->Clean();
	/* ȥ��this->Clean();�����ɣ�
	 * Inode::Clean()�ض�����IAlloc()������·���DiskInode��ԭ�����ݣ�
	 * �����ļ���Ϣ��Clean()�����в�Ӧ�����i_dev, i_number, i_flag, i_count,
	 * ���������ڴ�Inode����DiskInode�����ľ��ļ���Ϣ����Inode�๹�캯����Ҫ
	 * �����ʼ��Ϊ��Чֵ��
	 */

	/* ��Inode����ĳ�Ա������ʼ��Ϊ��Чֵ */
	this->i_flag = 0;			 /* ��Ч����־ */
	this->i_mode = 0;			 /* ��Ч���ļ�������ʽ��Ϣ */
	this->i_count = 0;			 /* ��Ч�����ü��� */
	this->i_nlink = 0;			 /* ��Ч�������� */
	this->i_dev = -1;			 /* ��Ч���豸�� */
	this->i_number = -1;		 /* ��Ч��i�ڵ�� */
	this->i_uid = -1;			 /* ��Ч���û�ID */
	this->i_gid = -1;			 /* ��Ч����ID */
	this->i_size = 0;			 /* ��Ч���ļ���С */
	this->i_lastr = -1;			 /* ��Ч������ȡ�������� */
	for (int i = 0; i < 10; i++) /* ��Ч������������� */
	{
		this->i_addr[i] = 0;
	}
}

Inode::~Inode()
{
	// nothing to do here
}

/* ���������ڸ���Inode�����е�������̿���������ȡ��Ӧ�ļ����� */
void Inode::ReadI()
{
	int lbn;													   /* �ļ��߼���� */
	int bn;														   /* lbn��Ӧ�������̿�� */
	int offset;													   /* ��ǰ�ַ�������ʼ����λ�� */
	int nbytes;													   /* �������û�Ŀ�����ֽ����� */
	short dev;													   /* �豸�� */
	Buf *pBuf;													   /* ������ָ�� */
	User &u = Kernel::Instance().GetUser();						   /* ��ȡ��ǰ�û�User�ṹ */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* ��ȡ������������ */
	DeviceManager &devMgr = Kernel::Instance().GetDeviceManager(); /* ��ȡ�豸������ */

	if (0 == u.u_IOParam.m_Count)
	{
		/* ��Ҫ���ֽ���Ϊ�㣬�򷵻� */
		return;
	}

	this->i_flag |= Inode::IACC; /* ���÷��ʱ�־��д��ʱ��Ҫ�޸ķ���ʱ�� */

	/* ������ַ��豸�ļ� ���������������*/
	if ((this->i_mode & Inode::IFMT) == Inode::IFCHR)
	{
		short major = Utility::GetMajor(this->i_addr[0]); /* ��ȡ�豸���е����豸�� */

		devMgr.GetCharDevice(major).Read(this->i_addr[0]); /* ������������� */
		return;											   /* ����󷵻� */
	}

	/* һ��һ���ַ���ض�������ȫ�����ݣ�ֱ�������ļ�β */
	while (User::NOERROR == u.u_error && u.u_IOParam.m_Count != 0)
	{
		/* ���㵱ǰ�ַ������ļ��е��߼����lbn���Լ����ַ����е���ʼλ��offset */
		lbn = bn = u.u_IOParam.m_Offset / Inode::BLOCK_SIZE;
		offset = u.u_IOParam.m_Offset % Inode::BLOCK_SIZE;
		/* ���͵��û������ֽ�������ȡ�������ʣ���ֽ����뵱ǰ�ַ�������Ч�ֽ�����Сֵ */
		nbytes = Utility::Min(Inode::BLOCK_SIZE - offset /* ������Ч�ֽ��� */, u.u_IOParam.m_Count);

		if ((this->i_mode & Inode::IFMT) != Inode::IFBLK)
		{ /* �������������豸�ļ� */

			int remain = this->i_size - u.u_IOParam.m_Offset;
			/* ����Ѷ��������ļ���β */
			if (remain <= 0)
			{
				return; /* ����󷵻� */
			}
			/* ���͵��ֽ�������ȡ����ʣ���ļ��ĳ��� */
			nbytes = Utility::Min(nbytes, remain);

			/* ���߼����lbnת���������̿��bn ��Bmap������Inode::rablock����UNIX��Ϊ��ȡԤ����Ŀ���̫��ʱ��
			 * �����Ԥ������ʱ Inode::rablock ֵΪ 0��
			 * */
			if ((bn = this->Bmap(lbn)) == 0)
			{
				/* ����߼����lbn��Ӧ�������̿��bnΪ0���򷵻� */
				return;
			}
			dev = this->i_dev; /* �������������豸�ļ������豸��Ϊi_dev */
		}
		else /* �����������豸�ļ� */
		{
			dev = this->i_addr[0];	 /* ������豸�ļ�i_addr[0]�д�ŵ����豸�� */
			Inode::rablock = bn + 1; /* Ԥ����һ�� */
		}

		if (this->i_lastr + 1 == lbn) /* �����˳����������Ԥ�� */
		{
			/* ����ǰ�飬��Ԥ����һ�� */
			pBuf = bufMgr.Breada(dev, bn, Inode::rablock);
		}
		else
		{
			/* ����˳�����ֻ����ǰ�� */
			pBuf = bufMgr.Bread(dev, bn); /* ��ȡ�����̿��bn��Ӧ�Ļ����� */
		}
		/* ��¼�����ȡ�ַ�����߼���� */
		this->i_lastr = lbn;

		/* ������������ʼ��λ�� */
		unsigned char *start = pBuf->b_addr + offset;

		/* ������: �ӻ������������û�Ŀ����
		 * i386оƬ��ͬһ��ҳ��ӳ���û��ռ���ں˿ռ䣬��һ��Ӳ���ϵĲ��� ʹ��i386��ʵ�� iomove����
		 * ��PDP-11Ҫ�������*/
		Utility::IOMove(start, u.u_IOParam.m_Base, nbytes);

		/* �ô����ֽ���nbytes���¶�дλ�� */
		u.u_IOParam.m_Base += nbytes;	/* �����û�Ŀ���� */
		u.u_IOParam.m_Offset += nbytes; /* ���¶�дλ�� */
		u.u_IOParam.m_Count -= nbytes;	/* ����ʣ���ֽ��� */

		bufMgr.Brelse(pBuf); /* ʹ���껺�棬�ͷŸ���Դ */
	}
}

/*����Inode�����е�������̿�������������д���ļ�*/
void Inode::WriteI()
{
	int lbn;													   /* �ļ��߼���� */
	int bn;														   /* lbn��Ӧ�������̿�� */
	int offset;													   /* ��ǰ�ַ�������ʼ����λ�� */
	int nbytes;													   /* �����ֽ����� */
	short dev;													   /* �豸�� */
	Buf *pBuf;													   /* ����ָ�� */
	User &u = Kernel::Instance().GetUser();						   /* ��ȡ�û�user���ݽṹ */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* ��ȡ��������� */
	DeviceManager &devMgr = Kernel::Instance().GetDeviceManager(); /* ��ȡ�豸������ */

	/* ����Inode�����ʱ�־λ */
	this->i_flag |= (Inode::IACC | Inode::IUPD); /* ���ñ����ʱ�־λ�ͱ��޸ı�־λ */

	/* ���ַ��豸�ķ��� */
	if ((this->i_mode & Inode::IFMT) == Inode::IFCHR)
	{
		/* ������ַ��豸�ļ�����ֱ�ӵ����ַ��豸��Write���� */
		short major = Utility::GetMajor(this->i_addr[0]); /* ��ȡ�豸�� */

		devMgr.GetCharDevice(major).Write(this->i_addr[0]); /* �����ַ��豸��Write���� */
		return;												/* ���� */
	}

	if (0 == u.u_IOParam.m_Count)
	{
		/* ��Ҫ���ֽ���Ϊ�㣬�򷵻� */
		return;
	}

	while (User::NOERROR == u.u_error && u.u_IOParam.m_Count != 0)
	{
		/* ���㵱ǰ�ַ�����߼����lbn���Լ��ڸ��ַ����е���ʼ����λ��offset */
		lbn = u.u_IOParam.m_Offset / Inode::BLOCK_SIZE;
		offset = u.u_IOParam.m_Offset % Inode::BLOCK_SIZE;
		/* ���㴫���ֽ�����nbytes,�Ǹÿ���ʣ���С���û�Ҫ���͵Ĵ�С�Ľ�Сֵ */
		nbytes = Utility::Min(Inode::BLOCK_SIZE - offset, u.u_IOParam.m_Count);

		if ((this->i_mode & Inode::IFMT) != Inode::IFBLK)
		{ /* ��ͨ�ļ� */

			/* ���߼����lbnת���������̿��bn */
			if ((bn = this->Bmap(lbn)) == 0)
			{
				return; /* ����߼����lbn��Ӧ�������̿��bnΪ0���򷵻� */
			}
			dev = this->i_dev; /* �豸����Ϊi_dev */
		}
		else
		{ /* ���豸�ļ���Ҳ����Ӳ�� */
			dev = this->i_addr[0];
		}

		if (Inode::BLOCK_SIZE == nbytes)
		{
			/* ���д������������һ���ַ��飬��Ϊ����仺�� */
			pBuf = bufMgr.GetBlk(dev, bn);
		}
		else
		{
			/* д�����ݲ���һ���ַ��飬�ȶ���д���������ַ����Ա�������Ҫ��д�����ݣ� */
			pBuf = bufMgr.Bread(dev, bn);
		}

		/* ���������ݵ���ʼдλ�� */
		unsigned char *start = pBuf->b_addr + offset;

		/* д����: ���û�Ŀ�����������ݵ������� */
		Utility::IOMove(u.u_IOParam.m_Base, start, nbytes);

		/* �ô����ֽ���nbytes���¶�дλ��,�޸�user�ṹ�е�IO�����ṹ */
		u.u_IOParam.m_Base += nbytes;	/* �����û�Ŀ���� */
		u.u_IOParam.m_Offset += nbytes; /* ���¶�дλ�� */
		u.u_IOParam.m_Count -= nbytes;	/* ����ʣ���ֽ��� */

		if (u.u_error != User::NOERROR) /* д�����г��� */
		{
			bufMgr.Brelse(pBuf); /* �ͷŻ��� */
		}
		else if ((u.u_IOParam.m_Offset % Inode::BLOCK_SIZE) == 0) /* ���д��һ���ַ��� */
		{
			/* ���첽��ʽ���ַ���д����̣����̲���ȴ�I/O�������������Լ�������ִ�� */
			bufMgr.Bawrite(pBuf);
		}
		else /* ���������δд�� */
		{
			/* ��������Ϊ�ӳ�д�������ڽ���I/O�������ַ�������������� */
			bufMgr.Bdwrite(pBuf);
		}

		/* ��ͨ�ļ��������� */
		if ((this->i_size < u.u_IOParam.m_Offset) && (this->i_mode & (Inode::IFBLK & Inode::IFCHR)) == 0)
		{
			/* ����ļ�����С��д��λ�ã�������ļ����� */
			this->i_size = u.u_IOParam.m_Offset;
		}

		/*
		 * ֮ǰ�����ж��̿��ܵ��½����л����ڽ���˯���ڼ䵱ǰ�ڴ�Inode����
		 * ��ͬ�������Inode���ڴ���Ҫ�������ø��±�־λ��
		 * ����û�б�Ҫѽ����ʹwriteϵͳ����û��������iput����i_count����0֮��ŻὫ�ڴ�i�ڵ�ͬ���ش��̡�������
		 * �ļ�û��close֮ǰ�ǲ��ᷢ���ġ�
		 * ���ǵ�ϵͳ��writeϵͳ���������͸������ܳ�����������ˡ�
		 * ��������ȥ����
		 */
		this->i_flag |= Inode::IUPD;
	}
}

/*���������ڽ��ļ����߼����ת���ɶ�Ӧ�������̿��*/
int Inode::Bmap(int lbn)
{
	Buf *pFirstBuf;												   /* ���ڷ���һ�μ�������� */
	Buf *pSecondBuf;											   /* ���ڷ��ʶ��μ�������� */
	int phyBlkno;												   /* ת����������̿�� */
	int *iTable;												   /* ���ڷ��������̿���һ�μ�ӡ����μ�������� */
	int index;													   /* ���ڷ��������̿���һ�μ�ӡ����μ���������е�����ֵ */
	User &u = Kernel::Instance().GetUser();						   /* ��ȡ��ǰ�û� */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* ��ȡ������������ */
	FileSystem &fileSys = Kernel::Instance().GetFileSystem();	   /* ��ȡ�ļ�ϵͳ */

	/*
	 * Unix V6++���ļ������ṹ��(С�͡����ͺ;����ļ�)
	 * (1) i_addr[0] - i_addr[5]Ϊֱ���������ļ����ȷ�Χ��0 - 6���̿飻
	 *
	 * (2) i_addr[6] - i_addr[7]���һ�μ�����������ڴ��̿�ţ�ÿ���̿�
	 * �ϴ��128���ļ������̿�ţ������ļ����ȷ�Χ��7 - (128 * 2 + 6)���̿飻
	 *
	 * (3) i_addr[8] - i_addr[9]��Ŷ��μ�����������ڴ��̿�ţ�ÿ�����μ��
	 * �������¼128��һ�μ�����������ڴ��̿�ţ������ļ����ȷ�Χ��
	 * (128 * 2 + 6 ) < size <= (128 * 128 * 2 + 128 * 2 + 6)
	 */

	if (lbn >= Inode::HUGE_FILE_BLOCK)
	{
		/* ����ļ����ȳ��������ļ�����󳤶ȣ��򷵻ش��� */
		u.u_error = User::EFBIG; /* �ļ�̫�� */
		return 0;				 /* ����0 */
	}

	if (lbn < 6) /* �����С���ļ����ӻ���������i_addr[0-5]�л�������̿�ż��� */
	{
		phyBlkno = this->i_addr[lbn]; /* �ӻ����������л�������̿�� */

		/*
		 * ������߼���Ż�û����Ӧ�������̿����֮��Ӧ�������һ������顣
		 * ��ͨ�������ڶ��ļ���д�룬��д��λ�ó����ļ���С�����Ե�ǰ
		 * �ļ���������д�룬����Ҫ�������Ĵ��̿飬��Ϊ֮�����߼����
		 * �������̿��֮���ӳ�䡣
		 */
		if (phyBlkno == 0 && (pFirstBuf = fileSys.Alloc(this->i_dev)) != NULL)
		{
			/*
			 * ��Ϊ����ܿ������ϻ�Ҫ�õ��˴��·�������ݿ飬���Բ��������������
			 * �����ϣ����ǽ�������Ϊ�ӳ�д��ʽ���������Լ���ϵͳ��I/O������
			 */
			bufMgr.Bdwrite(pFirstBuf);	   /* ���·�������ݿ�д����� */
			phyBlkno = pFirstBuf->b_blkno; /* ����·�������ݿ�������̿�� */
			/* ���߼����lbnӳ�䵽�����̿��phyBlkno */
			this->i_addr[lbn] = phyBlkno; /* ���·�������ݿ�������̿��д����������� */
			this->i_flag |= Inode::IUPD;  /* ���ø��±�־λ */
		}
		/* �ҵ�Ԥ�����Ӧ�������̿�� */
		Inode::rablock = 0;
		if (lbn <= 4)
		{
			/*
			 * i_addr[0] - i_addr[5]Ϊֱ�����������Ԥ�����Ӧ�����ſ��Դ�
			 * ֱ���������л�ã����¼��Inode::rablock�С������Ҫ�����I/O����
			 * �����������飬���Եò�ֵ̫���ˡ�Ư����
			 *
			 */
			Inode::rablock = this->i_addr[lbn + 1];
		}
		/* ��ʱ����ļ���С�ͣ���Inode::rablockΪ0���ò��������ڶ�д�ļ�ʱ������ */
		return phyBlkno; /* ���������̿�� */
	}
	else /* lbn >= 6 ���͡������ļ� */
	{
		/* �����߼����lbn��Ӧi_addr[]�е����� */

		if (lbn < Inode::LARGE_FILE_BLOCK) /* �����ļ�: ���Ƚ���7 - (128 * 2 + 6)���̿�֮�� */
		{
			index = (lbn - Inode::SMALL_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK + 6; /* �������� */
		}
		else /* �����ļ�: ���Ƚ���263 - (128 * 128 * 2 + 128 * 2 + 6)���̿�֮�� */
		{
			index = (lbn - Inode::LARGE_FILE_BLOCK) / (Inode::ADDRESS_PER_INDEX_BLOCK * Inode::ADDRESS_PER_INDEX_BLOCK) + 8; /* �������� */
		}

		phyBlkno = this->i_addr[index];
		/* ������Ϊ�㣬���ʾ��������Ӧ�ļ��������� */
		if (0 == phyBlkno)
		{
			this->i_flag |= Inode::IUPD;
			/* ����һ�����̿��ż�������� */
			if ((pFirstBuf = fileSys.Alloc(this->i_dev)) == NULL)
			{
				return 0; /* ����ʧ�� */
			}
			/* i_addr[index]�м�¼���������������̿�� */
			this->i_addr[index] = pFirstBuf->b_blkno;
		}
		else
		{
			/* �����洢�����������ַ��� */
			pFirstBuf = bufMgr.Bread(this->i_dev, phyBlkno);
		}
		/* ��ȡ��������ַ */
		iTable = (int *)pFirstBuf->b_addr;

		if (index >= 8) /* ASSERT: 8 <= index <= 9 */
		{
			/*
			 * ���ھ����ļ��������pFirstBuf���Ƕ��μ��������
			 * ��������߼���ţ����ɶ��μ���������ҵ�һ�μ��������
			 */
			index = ((lbn - Inode::LARGE_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK; /* �������� */

			/* iTableָ�򻺴��еĶ��μ������������Ϊ�㣬������һ�μ�������� */
			phyBlkno = iTable[index]; /* һ�μ��������������̿�� */
			if (0 == phyBlkno)
			{
				/* �����һ�μ����������δ�����䣬����һ�����̿���һ�μ�������� */
				if ((pSecondBuf = fileSys.Alloc(this->i_dev)) == NULL)
				{
					/* ����һ�μ����������̿�ʧ�ܣ��ͷŻ����еĶ��μ��������Ȼ�󷵻� */
					bufMgr.Brelse(pFirstBuf);
					return 0;
				}
				/* ���·����һ�μ����������̿�ţ�������μ����������Ӧ�� */
				iTable[index] = pSecondBuf->b_blkno;
				/* �����ĺ�Ķ��μ���������ӳ�д��ʽ��������� */
				bufMgr.Bdwrite(pFirstBuf);
			}
			else
			{
				/* �ͷŶ��μ��������ռ�õĻ��棬������һ�μ�������� */
				bufMgr.Brelse(pFirstBuf);
				pSecondBuf = bufMgr.Bread(this->i_dev, phyBlkno);
			}

			/* ��pFirstBufָ��һ�μ�������� */
			pFirstBuf = pSecondBuf;
			/* ��iTableָ��һ�μ�������� */
			iTable = (int *)pSecondBuf->b_addr;
		}

		/* �����߼����lbn����λ��һ�μ���������еı������index */

		if (lbn < Inode::LARGE_FILE_BLOCK)
		{
			index = (lbn - Inode::SMALL_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK; /* �������� */
		}
		else
		{
			index = (lbn - Inode::LARGE_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK; /* �������� */
		}

		if ((phyBlkno = iTable[index]) == 0 && (pSecondBuf = fileSys.Alloc(this->i_dev)) != NULL)
		{
			/* �����䵽���ļ������̿�ŵǼ���һ�μ���������� */
			phyBlkno = pSecondBuf->b_blkno;
			iTable[index] = phyBlkno;
			/* �������̿顢���ĺ��һ�μ�����������ӳ�д��ʽ��������� */
			bufMgr.Bdwrite(pSecondBuf);
			bufMgr.Bdwrite(pFirstBuf);
		}
		else
		{
			/* �ͷ�һ�μ��������ռ�û��� */
			bufMgr.Brelse(pFirstBuf);
		}
		/* �ҵ�Ԥ�����Ӧ�������̿�ţ������ȡԤ�������Ҫ�����һ��for����������IO�������㣬���� */
		Inode::rablock = 0;
		/* ���Ԥ����Ų������һ����������飬��ȡԤ����� */
		if (index + 1 < Inode::ADDRESS_PER_INDEX_BLOCK)
		{
			/* �����洢�����������ַ��� */
			Inode::rablock = iTable[index + 1];
		}
		return phyBlkno; /* ����Ԥ�����Ӧ�������̿�� */
	}
}

/*�������ַ��豸�����豸�ļ������ø��豸ע���ڿ��豸���ر��е��豸��ʼ������*/
void Inode::OpenI(int mode)
{
	short dev;													   /* �豸�� */
	DeviceManager &devMgr = Kernel::Instance().GetDeviceManager(); /* �豸������ */
	User &u = Kernel::Instance().GetUser();						   /* �û������� */

	/*
	 * ����������豸���ַ��豸�ļ���i_addr[]������
	 * ���̿��������addr[0]�д�����豸��dev
	 */
	dev = this->i_addr[0]; /* ��ȡ�豸�� */

	/* ��ȡ���豸�� */
	short major = Utility::GetMajor(dev); /* ��ȡ���豸�� */

	/* �����ļ����ͣ�������Ӧ���豸��ʼ������ */
	switch (this->i_mode & Inode::IFMT)
	{
	case Inode::IFCHR: /* �ַ��豸���������ļ� */
		if (major >= devMgr.GetNChrDev())
		{
			u.u_error = User::ENXIO; /* no such device */
			return;
		}
		devMgr.GetCharDevice(major).Open(dev, mode);
		break;

	case Inode::IFBLK: /* ���豸���������ļ� */
		/* ����豸���Ƿ񳬳�ϵͳ�п��豸���� */
		if (major >= devMgr.GetNBlkDev())
		{
			u.u_error = User::ENXIO; /* no such device */
			return;
		}
		/* �������豸�Ż�ȡ��Ӧ�Ŀ��豸BlockDevice�������� */
		BlockDevice &bdev = devMgr.GetBlockDevice(major);
		/* ���ø��豸���ض���ʼ���߼� */
		bdev.Open(dev, mode);
		break;
	}

	return;
}

/* �������ַ��豸�����豸�ļ�������Ը��豸�����ü���Ϊ0������ø��豸�Ĺرճ��� */
void Inode::CloseI(int mode)
{
	short dev;													   /* �豸�� */
	DeviceManager &devMgr = Kernel::Instance().GetDeviceManager(); /* �豸������ */

	/* addr[0]�д�����豸��dev */
	dev = this->i_addr[0]; /* ��ȡ�豸�� */

	short major = Utility::GetMajor(dev); /* ��ȡ���豸�� */

	/* ����ʹ�ø��ļ�,�ر������ļ� */
	if (this->i_count <= 1)
	{
		/* �����ļ����ͣ�������Ӧ���豸�رճ��� */
		switch (this->i_mode & Inode::IFMT)
		{
		case Inode::IFCHR:
			devMgr.GetCharDevice(major).Close(dev, mode); /* ���ø��豸���ض��ر��߼� */
			break;

		case Inode::IFBLK:
			/* �������豸�Ż�ȡ��Ӧ�Ŀ��豸BlockDevice�������� */
			BlockDevice &bdev = devMgr.GetBlockDevice(major);
			/* ���ø��豸���ض��ر��߼� */
			bdev.Close(dev, mode);
			break;
		}
	}
}

/* ���������ڸ������Inode�����ķ���ʱ�䡢�޸�ʱ��*/
void Inode::IUpdate(int time)
{
	Buf *pBuf;													   /* �����ָ�� */
	DiskInode dInode;											   /* ����Inode */
	FileSystem &filesys = Kernel::Instance().GetFileSystem();	   /* �ļ�ϵͳ */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* ��������� */

	/* ��IUPD��IACC��־֮һ�����ã�����Ҫ������ӦDiskInode
	 * Ŀ¼����������������;����Ŀ¼�ļ���IACC��IUPD��־ */
	if ((this->i_flag & (Inode::IUPD | Inode::IACC)) != 0)
	{
		if (filesys.GetFS(this->i_dev)->s_ronly != 0)
		{
			/* ������ļ�ϵͳֻ�� */
			return;
		}

		/* ���ص�ע�ͣ��ڻ�������ҵ�������i�ڵ㣨this->i_number���Ļ����
		 * ����һ�������Ļ���飬���δ����е�Bwrite()�ڽ������д�ش��̺���ͷŸû���顣
		 * ���ô�Ÿ�DiskInode���ַ�����뻺���� */
		pBuf = bufMgr.Bread(this->i_dev, FileSystem::INODE_ZONE_START_SECTOR + this->i_number / FileSystem::INODE_NUMBER_PER_SECTOR); /* ���������Inode�Ļ���� */

		/* ���ڴ�Inode�����е���Ϣ���Ƶ�dInode�У�Ȼ��dInode���ǻ����оɵ����Inode */
		dInode.d_mode = this->i_mode;	/* �ļ����ͺͷ���Ȩ�� */
		dInode.d_nlink = this->i_nlink; /* ������ */
		dInode.d_uid = this->i_uid;		/* �ļ������ߵ��û���ʶ�� */
		dInode.d_gid = this->i_gid;		/* �ļ������ߵ����ʶ�� */
		dInode.d_size = this->i_size;	/* �ļ����� */
		for (int i = 0; i < 10; i++)
		{
			dInode.d_addr[i] = this->i_addr[i]; /* �ļ�ռ�õĴ��̿�� */
		}
		if (this->i_flag & Inode::IACC)
		{
			/* ����������ʱ�� */
			dInode.d_atime = time;
		}
		if (this->i_flag & Inode::IUPD)
		{
			/* ����������ʱ�� */
			dInode.d_mtime = time;
		}

		/* ��pָ�򻺴����о����Inode��ƫ��λ�� */
		unsigned char *p = pBuf->b_addr + (this->i_number % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
		DiskInode *pNode = &dInode;

		/* ��dInode�е������ݸ��ǻ����еľ����Inode */
		Utility::DWordCopy((int *)pNode, (int *)p, sizeof(DiskInode) / sizeof(int));

		/* ������д�������̣��ﵽ���¾����Inode��Ŀ�� */
		bufMgr.Bwrite(pBuf);
	}
}
/* �����������ͷ�Inode��ռ�õĴ��̿� */
void Inode::ITrunc()
{
	/* ���ɴ��̸��ٻ����ȡ���һ�μ�ӡ����μ��������Ĵ��̿� */
	BufferManager &bm = Kernel::Instance().GetBufferManager();
	/* ��ȡg_FileSystem��������ã�ִ���ͷŴ��̿�Ĳ��� */
	FileSystem &filesys = Kernel::Instance().GetFileSystem();

	/* ������ַ��豸���߿��豸���˳� */
	if (this->i_mode & (Inode::IFCHR & Inode::IFBLK))
	{
		return;
	}

	/* ����FILO��ʽ�ͷţ��Ծ���ʹ��SuperBlock�м�¼�Ŀ����̿��������
	 *
	 * Unix V6++���ļ������ṹ��(С�͡����ͺ;����ļ�)
	 * (1) i_addr[0] - i_addr[5]Ϊֱ���������ļ����ȷ�Χ��0 - 6���̿飻
	 *
	 * (2) i_addr[6] - i_addr[7]���һ�μ�����������ڴ��̿�ţ�ÿ���̿�
	 * �ϴ��128���ļ������̿�ţ������ļ����ȷ�Χ��7 - (128 * 2 + 6)���̿飻
	 *
	 * (3) i_addr[8] - i_addr[9]��Ŷ��μ�����������ڴ��̿�ţ�ÿ�����μ��
	 * �������¼128��һ�μ�����������ڴ��̿�ţ������ļ����ȷ�Χ��
	 * (128 * 2 + 6 ) < size <= (128 * 128 * 2 + 128 * 2 + 6)
	 */
	for (int i = 9; i >= 0; i--) /* ��i_addr[9]��i_addr[0] */
	{
		/* ���i_addr[]�е�i��������� */
		if (this->i_addr[i] != 0)
		{
			/* �����i_addr[]�е�һ�μ�ӡ����μ�������� */
			if (i >= 6 && i <= 9)
			{
				/* �������������뻺�� */
				Buf *pFirstBuf = bm.Bread(this->i_dev, this->i_addr[i]);
				/* ��ȡ��������ַ */
				int *pFirst = (int *)pFirstBuf->b_addr;

				/* ÿ�ż���������¼ 512/sizeof(int) = 128�����̿�ţ�������ȫ��128�����̿� */
				for (int j = 128 - 1; j >= 0; j--)
				{
					if (pFirst[j] != 0) /* �������������� */
					{
						/*
						 * ��������μ��������i_addr[8]��i_addr[9]�
						 * ��ô���ַ����¼����128��һ�μ���������ŵĴ��̿��
						 */
						if (i >= 8 && i <= 9)
						{
							Buf *pSecondBuf = bm.Bread(this->i_dev, pFirst[j]); /* ��һ�μ����������뻺��,ͨ��ѭ�����������ͷ� */
							int *pSecond = (int *)pSecondBuf->b_addr;			/* ��ȡ��������ַ */

							for (int k = 128 - 1; k >= 0; k--)
							{
								if (pSecond[k] != 0)
								{
									/* �ͷ�ָ���Ĵ��̿� */
									filesys.Free(this->i_dev, pSecond[k]);
								}
							}
							/* ����ʹ����ϣ��ͷ��Ա㱻��������ʹ�� */
							bm.Brelse(pSecondBuf);
						}
						/* �ͷ�ָ���Ĵ��̿� */
						filesys.Free(this->i_dev, pFirst[j]);
					}
				}
				/* ����ʹ����ϣ��ͷ��Ա㱻��������ʹ�� */
				bm.Brelse(pFirstBuf);
			}
			/* �ͷ���������ռ�õĴ��̿� */
			filesys.Free(this->i_dev, this->i_addr[i]);
			/* 0��ʾ����������� */
			this->i_addr[i] = 0;
		}
	}

	/* �̿��ͷ���ϣ��ļ���С���� */
	this->i_size = 0;
	/* ����IUPD��־λ����ʾ���ڴ�Inode��Ҫͬ������Ӧ���Inode */
	this->i_flag |= Inode::IUPD;
	/* ����ļ���־ ��ԭ����RWXRWXRWX����*/
	this->i_mode &= ~(Inode::ILARG & Inode::IRWXU & Inode::IRWXG & Inode::IRWXO);
	/* �ͷ��ڴ�Inode */
	this->i_nlink = 1;
}

/* ���������ڶ�Pipe����Inode���������һ�����ȴ�����˯�ߵĽ���*/
void Inode::NFrele()
{
	/* ����pipe��Inode,���һ�����Ӧ���� */
	this->i_flag &= ~Inode::ILOCK;

	/* ����н����ڵȴ���Inode���������� */
	if (this->i_flag & Inode::IWANT)
	{
		/* ����ȴ���־λ */
		this->i_flag &= ~Inode::IWANT;
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)this); /* �������еȴ���Inode�Ľ��� */
	}
}

/*��Pipe���������Pipe�Ѿ���������������IWANT��־��˯�ߵȴ�ֱ������*/
void Inode::NFlock()
{
	User &u = Kernel::Instance().GetUser(); /* ��ȡ��ǰ���̵�User�ṹ */

	/* ���Pipe�Ѿ���������������IWANT��־��˯�ߵȴ�ֱ������ */
	while (this->i_flag & Inode::ILOCK)
	{
		this->i_flag |= Inode::IWANT;								   /* ����IWANT��־ */
		u.u_procp->Sleep((unsigned long)this, ProcessManager::PRIBIO); /* ˯�ߵȴ�ֱ������ */
	}
	this->i_flag |= Inode::ILOCK; /* ����ILOCK��־ */
}

/* ���������ڶ�Pipe���������һ�����ȴ�����˯�ߵĽ���*/
void Inode::Prele()
{
	/* ����pipe��Inode,���һ�����Ӧ���� */
	this->i_flag &= ~Inode::ILOCK;

	if (this->i_flag & Inode::IWANT)
	{
		/* ����н����ڵȴ���Inode���������� */
		this->i_flag &= ~Inode::IWANT;										   /* ����ȴ���־λ */
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)this); /* �������еȴ���Inode�Ľ��� */
	}
}

/*��Pipe���������Pipe�Ѿ���������������IWANT��־��˯�ߵȴ�ֱ������*/
void Inode::Plock()
{
	/* ��ȡ��ǰ���̵�User�ṹ */
	User &u = Kernel::Instance().GetUser();

	/* ���Pipe�Ѿ���������������IWANT��־��˯�ߵȴ�ֱ������ */
	while (this->i_flag & Inode::ILOCK)
	{
		this->i_flag |= Inode::IWANT; /* ����IWANT��־ */
		u.u_procp->Sleep((unsigned long)this, ProcessManager::PPIPE);
		/* ˯�ߵȴ�ֱ������,ע��˴���˯��ԭ��ΪPPIPE */
	}
	this->i_flag |= Inode::ILOCK; /* ����ILOCK��־ */
}

/*�������������Inode�����е����ݣ���������i_dev, i_number, i_flag, i_count*/
void Inode::Clean()
{
	/*
	 * Inode::Clean()�ض�����IAlloc()������·���DiskInode��ԭ�����ݣ�
	 * �����ļ���Ϣ��Clean()�����в�Ӧ�����i_dev, i_number, i_flag, i_count,
	 * ���������ڴ�Inode����DiskInode�����ľ��ļ���Ϣ����Inode�๹�캯����Ҫ
	 * �����ʼ��Ϊ��Чֵ��
	 */

	// this->i_flag = 0;
	this->i_mode = 0; /* ����ļ����ͺͷ���Ȩ�� */
	// this->i_count = 0;
	this->i_nlink = 0; /* ����ļ������� */
	// this->i_dev = -1;
	// this->i_number = -1;
	this->i_uid = -1;	/* ����ļ������� */
	this->i_gid = -1;	/* ����ļ��������� */
	this->i_size = 0;	/* ����ļ���С */
	this->i_lastr = -1; /* ������һ�ζ�ȡ���ļ���� */
	for (int i = 0; i < 10; i++)
	{
		this->i_addr[i] = 0; /* ����ļ����ݿ�� */
	}
}

/* ���������ڽ��ڴ�Inode�е���Ϣ���������Inode�� */
void Inode::ICopy(Buf *bp, int inumber)
{
	DiskInode dInode;			/* ��ʱ���������ڴ�����Inode��Ϣ */
	DiskInode *pNode = &dInode; /* ָ����ʱ����dInode */

	/* ��pָ�򻺴����б��Ϊinumber���Inode��ƫ��λ�� */
	unsigned char *p = bp->b_addr + (inumber % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
	/* �����������Inode���ݿ�������ʱ����dInode�У���4�ֽڿ��� */
	Utility::DWordCopy((int *)p, (int *)pNode, sizeof(DiskInode) / sizeof(int));

	/* �����Inode����dInode����Ϣ���Ƶ��ڴ�Inode�� */
	this->i_mode = dInode.d_mode;	/* �ļ����ͺͷ���Ȩ�� */
	this->i_nlink = dInode.d_nlink; /* �ļ������� */
	this->i_uid = dInode.d_uid;		/* �ļ������� */
	this->i_gid = dInode.d_gid;		/* �ļ��������� */
	this->i_size = dInode.d_size;	/* �ļ���С */
	for (int i = 0; i < 10; i++)
	{
		this->i_addr[i] = dInode.d_addr[i]; /* �ļ����ݿ�� */
	}
}

/*============================class DiskInode=================================*/
/*
 * ��������ڵ�(DiskINode)�Ķ���
 * ���Inodeλ���ļ��洢�豸�ϵ�
 * ���Inode���С�ÿ���ļ���Ψһ��Ӧ
 * �����Inode���������Ǽ�¼�˸��ļ�
 * ��Ӧ�Ŀ�����Ϣ��
 * ���Inode������ֶκ��ڴ�Inode���ֶ�
 * ���Ӧ�����INode���󳤶�Ϊ64�ֽڣ�
 * ÿ�����̿���Դ��512/64 = 8�����Inode
 */
/* ���Inode���캯�����ڳ�ʼ��ջ���ڴ�ռ� */
DiskInode::DiskInode()
{
	/*
	 * ���DiskInodeû�й��캯�����ᷢ�����½��Ѳ���Ĵ���
	 * DiskInode��Ϊ�ֲ�����ռ�ݺ���Stack Frame�е��ڴ�ռ䣬����
	 * ��οռ�û�б���ȷ��ʼ�����Ծɱ�������ǰջ���ݣ����ڲ�����
	 * DiskInode�����ֶζ��ᱻ���£���DiskInodeд�ص�������ʱ������
	 * ����ǰջ����һͬд�أ�����д�ؽ������Ī����������ݡ�
	 * �����Ҫ��DiskInode���캯���н������ֶγ�ʼ��Ϊ��Чֵ��
	 */
	this->d_mode = 0;  /* �ļ����ͺͷ���Ȩ�� */
	this->d_nlink = 0; /* �ļ������� */
	this->d_uid = -1;  /* �ļ������� */
	this->d_gid = -1;  /* �ļ��������� */
	this->d_size = 0;  /* �ļ���С */
	for (int i = 0; i < 10; i++)
	{
		this->d_addr[i] = 0; /* �ļ����ݿ�� */
	}
	this->d_atime = 0; /* �ļ����һ�η���ʱ�� */
	this->d_mtime = 0; /* �ļ����һ���޸�ʱ�� */
}

DiskInode::~DiskInode()
{
	// nothing to do here
}
