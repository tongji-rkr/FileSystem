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
/* ϵͳȫ�ִ��ļ������ʵ���Ķ��� */
OpenFileTable g_OpenFileTable;

/*���캯��*/
OpenFileTable::OpenFileTable()
{
	// nothing to do here
}

/*��������*/
OpenFileTable::~OpenFileTable()
{
	// nothing to do here
}

/*���ã����̴��ļ������������ҵĿ�����  ֮ �±�  д�� u_ar0[EAX]*/
File *OpenFileTable::FAlloc()
{
	int fd;									/* ���ڴ�Ŵ��ļ���������ֵ */
	User &u = Kernel::Instance().GetUser(); /*��õ�ǰ�û�User�ṹ*/

	/* �ڽ��̴��ļ����������л�ȡһ�������� */
	fd = u.u_ofiles.AllocFreeSlot();

	if (fd < 0) /* ���Ѱ�ҿ�����ʧ�� */
	{
		return NULL;
	}

	/* ��ϵͳȫ�ִ��ļ�����Ѱ��һ�������� */
	for (int i = 0; i < OpenFileTable::NFILE; i++)
	{
		/* f_count==0��ʾ������� */
		if (this->m_File[i].f_count == 0)
		{
			/* ������������File�ṹ�Ĺ�����ϵ */
			u.u_ofiles.SetF(fd, &this->m_File[i]);
			/* ���Ӷ�file�ṹ�����ü��� */
			this->m_File[i].f_count++;
			/* ����ļ�����дλ�� */
			this->m_File[i].f_offset = 0;
			/*��File�ṹ��ָ�뷵��*/
			return (&this->m_File[i]);
		}
	}

	/* ���û���ҵ��������ʾ����*/
	Diagnose::Write("No Free File Struct\n");
	/*���ô�����Ϊû���ҵ�������*/
	u.u_error = User::ENFILE;
	return NULL;
}

/*�Դ��ļ����ƿ�File�ṹ�����ü���f_count��1�������ü���f_countΪ0�����ͷ�File�ṹ��*/
void OpenFileTable::CloseF(File *pFile)
{
	Inode *pNode;													  /* ���ڴ���ļ���Ӧ��Inode�ṹ */
	ProcessManager &procMgr = Kernel::Instance().GetProcessManager(); /*��ý��̹�����������*/

	/* �ܵ����� */
	if (pFile->f_flag & File::FPIPE)
	{
		pNode = pFile->f_inode;							  /* ����ļ���Ӧ��Inode�ṹ */
		pNode->i_mode &= ~(Inode::IREAD | Inode::IWRITE); /* �����д��־ */
		procMgr.WakeUpAll((unsigned long)(pNode + 1));	  /* �������еȴ��ùܵ��Ľ��� */
		procMgr.WakeUpAll((unsigned long)(pNode + 2));	  /* �������еȴ��ùܵ��Ľ��� */
	}

	if (pFile->f_count <= 1)
	{
		/*
		 * �����ǰ���������һ�����ø��ļ��Ľ��̣�
		 * ��������豸���ַ��豸�ļ�������Ӧ�Ĺرպ���
		 */
		pFile->f_inode->CloseI(pFile->f_flag & File::FWRITE); /* ����Inode�ṹ��CloseI���� */
		g_InodeTable.IPut(pFile->f_inode);					  /* �ͷ�Inode�ṹ */
	}

	/* ���õ�ǰFile�Ľ�������1 */
	pFile->f_count--;
}

/*==============================class InodeTable===================================*/
/*  �����ڴ�Inode���ʵ�� */
InodeTable g_InodeTable;

/*���캯��*/
InodeTable::InodeTable()
{
	// nothing to do here
}

/*��������*/
InodeTable::~InodeTable()
{
	// nothing to do here
}

/*���ã���ʼ���ڴ�Inode��*/
void InodeTable::Initialize()
{
	/* ��ȡ��g_FileSystem������ */
	this->m_FileSystem = &Kernel::Instance().GetFileSystem(); /*����ļ�ϵͳ������*/
}

/*���ã�����ָ���豸dev��Inode���inumber�����ڴ�Inode���в��Ҷ�Ӧ���ڴ�Inode�ṹ�������Inode�Ѿ����ڴ��У��������������ظ��ڴ�Inode����������ڴ��У���������ڴ�����������ظ��ڴ�Inode*/
Inode *InodeTable::IGet(short dev, int inumber)
{
	Inode *pInode;							/* ���ڴ���ڴ�Inode�ṹ��ָ�� */
	User &u = Kernel::Instance().GetUser(); /*��õ�ǰ�û�User�ṹ*/

	while (true)
	{
		/* ���ָ���豸dev�б��Ϊinumber�����Inode�Ƿ����ڴ濽�� */
		int index = this->IsLoaded(dev, inumber);
		if (index >= 0) /* �ҵ��ڴ濽�� */
		{
			/* ����ڴ�Inode�ṹ��ָ�� */
			pInode = &(this->m_Inode[index]);
			/* ������ڴ�Inode������ */
			if (pInode->i_flag & Inode::ILOCK)
			{
				/* ����IWANT��־��Ȼ��˯�� */
				pInode->i_flag |= Inode::IWANT;
				/* ˯�ߣ��ȴ����ڴ�Inode���� */
				u.u_procp->Sleep((unsigned long)&pInode, ProcessManager::PINOD);

				/* �ص�whileѭ������Ҫ������������Ϊ���ڴ�Inode�����Ѿ�ʧЧ */
				continue;
			}

			/* ������ڴ�Inode�����������ļ�ϵͳ�����Ҹ�Inode��Ӧ��Mountװ��� */
			if (pInode->i_flag & Inode::IMOUNT)
			{
				/* ��ø�Inode��Ӧ��Mountװ��� */
				Mount *pMount = this->m_FileSystem->GetMount(pInode);
				if (NULL == pMount)
				{
					/* û���ҵ� */
					Utility::Panic("No Mount Tab...");
				}
				else
				{
					/* ��������Ϊ���ļ�ϵͳ�豸�š���Ŀ¼Inode��� */
					dev = pMount->m_dev;
					/* ��������Ϊ���ļ�ϵͳ��Ŀ¼Inode��� */
					inumber = FileSystem::ROOTINO;
					/* �ص�whileѭ��������dev��inumberֵ�������� */
					continue;
				}
			}

			/*
			 * ����ִ�е������ʾ���ڴ�Inode���ٻ������ҵ���Ӧ�ڴ�Inode��
			 * ���������ü���������ILOCK��־������֮
			 */
			pInode->i_count++;				/* �������ü��� */
			pInode->i_flag |= Inode::ILOCK; /* ����ILOCK��־ */
			return pInode;					/* �����ڴ�Inode�ṹ��ָ�� */
		}
		else /* û��Inode���ڴ濽���������һ�������ڴ�Inode */
		{
			pInode = this->GetFreeInode();
			/* ���ڴ�Inode���������������Inodeʧ�� */
			if (NULL == pInode)
			{
				/* ���������Ϣ�����ô���ţ�����NULL */
				Diagnose::Write("Inode Table Overflow !\n"); /* ���������Ϣ */
				u.u_error = User::ENFILE;					 /* ���ô����Ϊû�п���Inode */
				return NULL;								 /* ����NULL */
			}
			else /* �������Inode�ɹ��������Inode�����·�����ڴ�Inode */
			{
				/* �����µ��豸�š����Inode��ţ��������ü������������ڵ����� */
				pInode->i_dev = dev;		   /* �����豸�� */
				pInode->i_number = inumber;	   /* �������Inode��� */
				pInode->i_flag = Inode::ILOCK; /* ����ILOCK��־ */
				pInode->i_count++;			   /* �������ü��� */
				pInode->i_lastr = -1;		   /* ��ʼ�����һ�ζ�ȡ���߼����Ϊ-1 */

				BufferManager &bm = Kernel::Instance().GetBufferManager();
				/* �������Inode���뻺���� */
				Buf *pBuf = bm.Bread(dev, FileSystem::INODE_ZONE_START_SECTOR + inumber / FileSystem::INODE_NUMBER_PER_SECTOR); /* �������Inode */

				/* �������I/O���� */
				if (pBuf->b_flags & Buf::B_ERROR)
				{
					/* �ͷŻ��� */
					bm.Brelse(pBuf);
					/* �ͷ�ռ�ݵ��ڴ�Inode */
					this->IPut(pInode);
					return NULL;
				}

				/* ���������е����Inode��Ϣ�������·�����ڴ�Inode�� */
				pInode->ICopy(pBuf, inumber);
				/* �ͷŻ��� */
				bm.Brelse(pBuf);
				return pInode;
			}
		}
	}
	return NULL; /* GCC likes it! */
}

/* close�ļ�ʱ�����Iput
 *      ��Ҫ���Ĳ������ڴ�i�ڵ���� i_count--����Ϊ0���ͷ��ڴ� i�ڵ㡢���иĶ�д�ش���
 * �����ļ�;��������Ŀ¼�ļ������������󶼻�Iput���ڴ�i�ڵ㡣·�����ĵ�����2��·������һ���Ǹ�
 *   Ŀ¼�ļ�������������д������ļ�������ɾ��һ�������ļ���������������д���ɾ����Ŀ¼����ô
 *   	���뽫���Ŀ¼�ļ�����Ӧ���ڴ� i�ڵ�д�ش��̡�
 *   	���Ŀ¼�ļ������Ƿ��������ģ����Ǳ��뽫����i�ڵ�д�ش��̡�
 * */
void InodeTable::IPut(Inode *pNode)
{
	/* ��ǰ����Ϊ���ø��ڴ�Inode��Ψһ���̣���׼���ͷŸ��ڴ�Inode */
	if (pNode->i_count == 1)
	{
		/*
		 * ��������Ϊ�������ͷŹ����п�����Ϊ���̲�����ʹ�øý���˯�ߣ�
		 * ��ʱ�п�����һ�����̻�Ը��ڴ�Inode���в������⽫�п��ܵ��´���
		 */
		pNode->i_flag |= Inode::ILOCK;

		/* ���ļ��Ѿ�û��Ŀ¼·��ָ���� */
		if (pNode->i_nlink <= 0)
		{
			/* �ͷŸ��ļ�ռ�ݵ������̿� */
			pNode->ITrunc();
			pNode->i_mode = 0; /* �����ļ�����Ϊ��Ч���� */
			/* �ͷŶ�Ӧ�����Inode */
			this->m_FileSystem->IFree(pNode->i_dev, pNode->i_number);
		}

		/* �������Inode��Ϣ */
		pNode->IUpdate(Time::time);

		/* �����ڴ�Inode�����һ��ѵȴ����� */
		pNode->Prele();
		/* ����ڴ�Inode�����б�־λ */
		pNode->i_flag = 0;
		/* �����ڴ�inode���еı�־֮һ����һ����i_count == 0 */
		pNode->i_number = -1;
	}

	/* �����ڴ�Inode�����ü��������ѵȴ����� */
	pNode->i_count--;
	pNode->Prele();
}

/*�����б��޸Ĺ����ڴ�Inode���µ���Ӧ���Inode��*/
void InodeTable::UpdateInodeTable()
{
	/* �����ڴ�Inode�� */
	for (int i = 0; i < InodeTable::NINODE; i++)
	{
		/*
		 * ���Inode����û�б�����������ǰδ����������ʹ�ã�����ͬ�������Inode��
		 * ����count������0��count == 0��ζ�Ÿ��ڴ�Inodeδ���κδ��ļ����ã�����ͬ����
		 */
		if ((this->m_Inode[i].i_flag & Inode::ILOCK) == 0 && this->m_Inode[i].i_count != 0)
		{
			/* ���ڴ�Inode������ͬ�������Inode */
			this->m_Inode[i].i_flag |= Inode::ILOCK;
			this->m_Inode[i].IUpdate(Time::time);

			/* ���ڴ�Inode���� */
			this->m_Inode[i].Prele();
		}
	}
}

/* ����豸dev�ϱ��Ϊinumber�����inode�Ƿ����ڴ濽����������򷵻ظ��ڴ�Inode���ڴ�Inode���е�����*/
int InodeTable::IsLoaded(short dev, int inumber)
{
	/* Ѱ��ָ�����Inode���ڴ濽�� */
	for (int i = 0; i < InodeTable::NINODE; i++)
	{
		if (this->m_Inode[i].i_dev == dev && this->m_Inode[i].i_number == inumber && this->m_Inode[i].i_count != 0)
		{
			/* �ҵ��������ڴ�Inode���ڴ�Inode���е����� */
			return i;
		}
	}
	return -1;
}

/* ���ڴ�Inode���л�ȡһ�����е��ڴ�Inode */
Inode *InodeTable::GetFreeInode()
{
	for (int i = 0; i < InodeTable::NINODE; i++)
	{
		/* ������ڴ�Inode���ü���Ϊ�㣬���Inode��ʾ���� */
		if (this->m_Inode[i].i_count == 0)
		{
			/* ���ظÿ��е��ڴ�Inode */
			return &(this->m_Inode[i]);
		}
	}
	return NULL; /* Ѱ��ʧ�� */
}
