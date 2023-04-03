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
 * �ļ�������(FileManager)
 * ��װ���ļ�ϵͳ�ĸ���ϵͳ�����ں���̬�´�����̣�
 * ����ļ���Open()��Close()��Read()��Write()�ȵ�
 * ��װ�˶��ļ�ϵͳ���ʵľ���ϸ�ڡ�
 */

/* ���캯�� */
FileManager::FileManager()
{
	// nothing to do here
}

/* �������� */
FileManager::~FileManager()
{
	// nothing to do here
}

/* ��ʼ����ȫ�ֶ���*/
void FileManager::Initialize()
{
	this->m_FileSystem = &Kernel::Instance().GetFileSystem(); /* ��ȡ�ļ�ϵͳ�����ַ */

	this->m_InodeTable = &g_InodeTable;		  /* ��ȡ�ڴ�i�ڵ������ַ */
	this->m_OpenFileTable = &g_OpenFileTable; /* ��ȡ���ļ�������ַ */

	this->m_InodeTable->Initialize(); /* ��ʼ���ڴ�i�ڵ�� */
}

/*
 * ���ܣ����ļ�
 * Ч�����������ļ��ṹ���ڴ�i�ڵ㿪�� ��i_count Ϊ������i_count ++��
 * */
void FileManager::Open()
{
	Inode *pInode;							/* ָ���ڴ������ڵ��ָ�� */
	User &u = Kernel::Instance().GetUser(); /* ��ȡ�û����̶��� */

	pInode = this->NameI(NextChar, FileManager::OPEN); /* 0 = Open, not create */
	/* û���ҵ���Ӧ��Inode */
	if (NULL == pInode)
	{
		return; /* NameI()�Ѿ�������u.u_error��ֱ�ӷ��� */
	}
	this->Open1(pInode, u.u_arg[1], 0);
}

/*
 * ���ܣ�����һ���µ��ļ�
 * Ч�����������ļ��ṹ���ڴ�i�ڵ㿪�� ��i_count Ϊ������Ӧ���� 1��
 * */
void FileManager::Creat()
{
	Inode *pInode;																		 /* ָ���ڴ������ڵ��ָ�� */
	User &u = Kernel::Instance().GetUser();												 /* ��ȡ�û����̶��� */
	unsigned int newACCMode = u.u_arg[1] & (Inode::IRWXU | Inode::IRWXG | Inode::IRWXO); /*���ļ��ķ���Ȩ��*/

	/* ����Ŀ¼��ģʽΪ1����ʾ����������Ŀ¼����д�������� */
	pInode = this->NameI(NextChar, FileManager::CREATE);
	/* û���ҵ���Ӧ��Inode����NameI���� */
	if (NULL == pInode)
	{
		if (u.u_error)
			return;
		/* ����Inode */
		pInode = this->MakNode(newACCMode & (~Inode::ISVTX));
		/* ����ʧ�� */
		if (NULL == pInode)
		{
			return;
		}

		/*
		 * �����ϣ�������ֲ����ڣ�ʹ�ò���trf = 2������open1()��
		 * ����Ҫ����Ȩ�޼�飬��Ϊ�ոս������ļ���Ȩ�޺ʹ������mode
		 * ����ʾ��Ȩ��������һ���ġ�
		 */
		this->Open1(pInode, File::FWRITE, 2);
	}
	else
	{
		/* ���NameI()�������Ѿ�����Ҫ�������ļ�������ո��ļ������㷨ITrunc()����UIDû�иı�
		 * ԭ��UNIX��������������ļ�����ȥ�����½����ļ�һ����Ȼ�������ļ������ߺ����Ȩ��ʽû�䡣
		 * Ҳ����˵creatָ����RWX������Ч��
		 * ������Ϊ���ǲ�����ģ�Ӧ�øı䡣
		 * ���ڵ�ʵ�֣�creatָ����RWX������Ч */
		this->Open1(pInode, File::FWRITE, 1);
		pInode->i_mode |= newACCMode;
	}
}

/*
 * trf == 0��open����
 * trf == 1��creat���ã�creat�ļ���ʱ��������ͬ�ļ������ļ�
 * trf == 2��creat���ã�creat�ļ���ʱ��δ������ͬ�ļ������ļ��������ļ�����ʱ��һ������
 * mode���������ļ�ģʽ����ʾ�ļ������� ����д���Ƕ�д
 */
void FileManager::Open1(Inode *pInode, int mode, int trf)
{
	User &u = Kernel::Instance().GetUser(); /* ��ȡ�û����̶��� */

	/*
	 * ����ϣ�����ļ��Ѵ��ڵ�����£���trf == 0��trf == 1����Ȩ�޼��
	 * �����ϣ�������ֲ����ڣ���trf == 2������Ҫ����Ȩ�޼�飬��Ϊ�ս���
	 * ���ļ���Ȩ�޺ʹ���Ĳ���mode������ʾ��Ȩ��������һ���ġ�
	 */
	if (trf != 2)
	{
		/* ��Ϊ2������ļ����� */
		if (mode & File::FREAD)
		{
			/* ����Ȩ�� */
			this->Access(pInode, Inode::IREAD);
		}
		if (mode & File::FWRITE)
		{
			/* ���дȨ�� */
			this->Access(pInode, Inode::IWRITE);
			/* ϵͳ����ȥдĿ¼�ļ��ǲ������ */
			if ((pInode->i_mode & Inode::IFMT) == Inode::IFDIR)
			{
				/* ���ô����ΪEISDIR */
				u.u_error = User::EISDIR;
			}
		}
	}

	/* ����Ƿ��д��� */
	if (u.u_error)
	{
		/* �д����ͷ��ڴ�i�ڵ� */
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* ��creat�ļ���ʱ��������ͬ�ļ������ļ����ͷŸ��ļ���ռ�ݵ������̿� */
	if (1 == trf)
	{
		/* ����ļ����� */
		pInode->ITrunc();
	}

	/* ����inode!
	 * ����Ŀ¼�����漰�����Ĵ��̶�д�������ڼ���̻���˯��
	 * ��ˣ����̱������������漰��i�ڵ㡣�����NameI��ִ�е�IGet����������
	 * �����ˣ����������п��ܻ���������л��Ĳ��������Խ���i�ڵ㡣
	 */
	pInode->Prele();

	/* ������ļ����ƿ�File�ṹ */
	File *pFile = this->m_OpenFileTable->FAlloc();
	/* ����ʧ�� */
	if (NULL == pFile)
	{
		/* �ͷ��ڴ�i�ڵ� */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* ���ô��ļ���ʽ������File�ṹ���ڴ�Inode�Ĺ�����ϵ */
	pFile->f_flag = mode & (File::FREAD | File::FWRITE); /* ���ô��ļ���ʽ */
	pFile->f_inode = pInode;							 /* ����File�ṹ���ڴ�Inode�Ĺ�����ϵ */

	/* �����豸�򿪺��� */
	pInode->OpenI(mode & File::FWRITE);

	/* Ϊ�򿪻��ߴ����ļ��ĸ�����Դ���ѳɹ����䣬�������� */
	if (u.u_error == 0)
	{
		return;
	}
	else /* ����������ͷ���Դ */
	{
		/* �ͷŴ��ļ������� */
		int fd = u.u_ar0[User::EAX];
		if (fd != -1)
		{
			/* �ͷŴ��ļ�������fd���ݼ�File�ṹ���ü��� */
			u.u_ofiles.SetF(fd, NULL);
			/* �ݼ�File�ṹ��Inode�����ü��� ,File�ṹû���� f_countΪ0�����ͷ�File�ṹ��*/
			pFile->f_count--;
		}
		/* �ͷ��ڴ�i�ڵ� */
		this->m_InodeTable->IPut(pInode);
	}
}

/*�������������Close()ϵͳ���ô������*/
void FileManager::Close()
{
	User &u = Kernel::Instance().GetUser(); /* ��ȡ�û����̶��� */
	int fd = u.u_arg[0];					/* ��ȡ�ļ������� */

	/* ��ȡ���ļ����ƿ�File�ṹ */
	File *pFile = u.u_ofiles.GetF(fd);
	if (NULL == pFile)
	{
		return;
	}

	/* �ͷŴ��ļ�������fd���ݼ�File�ṹ���ü��� */
	u.u_ofiles.SetF(fd, NULL);
	/* �ݼ�File�ṹ��Inode�����ü��� ,File�ṹû���� f_countΪ0�����ͷ�File�ṹ��*/
	this->m_OpenFileTable->CloseF(pFile);
}

/* �������������Seek()ϵͳ���ô������ */
void FileManager::Seek()
{
	File *pFile;							/* �ļ�ָ�� */
	User &u = Kernel::Instance().GetUser(); /* ��ȡ�û����̶��� */
	int fd = u.u_arg[0];					/* ��ȡ�ļ������� */

	pFile = u.u_ofiles.GetF(fd); /* ��ȡ���ļ����ƿ�File�ṹ */
	if (NULL == pFile)
	{
		return; /* ��FILE�����ڣ�GetF��������� */
	}

	/* �ܵ��ļ�������seek */
	if (pFile->f_flag & File::FPIPE)
	{
		u.u_error = User::ESPIPE; /* ���ô����ΪESPIPE */
		return;
	}

	int offset = u.u_arg[1]; /* ��ȡƫ���� */

	/* ���u.u_arg[2]��3 ~ 5֮�䣬��ô���ȵ�λ���ֽڱ�Ϊ512�ֽ� */
	if (u.u_arg[2] > 2)
	{
		offset = offset << 9; /* ����512 */
		u.u_arg[2] -= 3;	  /* arg 2 ��ȥ3 */
	}

	switch (u.u_arg[2])
	{
	/* ��дλ������Ϊoffset */
	case 0:
		pFile->f_offset = offset;
		break;
	/* ��дλ�ü�offset(�����ɸ�) */
	case 1:
		pFile->f_offset += offset;
		break;
	/* ��дλ�õ���Ϊ�ļ����ȼ�offset */
	case 2:
		pFile->f_offset = pFile->f_inode->i_size + offset;
		break;
	}
}

/* �������������Dup()ϵͳ���ô������ */
void FileManager::Dup()
{
	File *pFile;							/* �ļ�ָ�� */
	User &u = Kernel::Instance().GetUser(); /* ��ȡ�û����̶��� */
	int fd = u.u_arg[0];					/* ��ȡ�ļ������� */

	pFile = u.u_ofiles.GetF(fd); /* ��ȡ���ļ����ƿ�File�ṹ */
	if (NULL == pFile)
	{
		/* ��ʧ��*/
		return;
	}

	int newFd = u.u_ofiles.AllocFreeSlot(); /* ������������ */
	if (newFd < 0)
	{
		/* ����ʧ�� */
		return;
	}
	/* ���˷�����������newFd�ɹ� */
	u.u_ofiles.SetF(newFd, pFile);
	pFile->f_count++; /* ����File�ṹ���ü��� */
}

/* ������������ɻ�ȡ�ļ���Ϣ������� */
void FileManager::FStat()
{
	File *pFile;							/* �ļ�ָ�� */
	User &u = Kernel::Instance().GetUser(); /* ��ȡ�û����̶��� */
	int fd = u.u_arg[0];					/* ��ȡ�ļ������� */

	pFile = u.u_ofiles.GetF(fd); /* ��ȡ���ļ����ƿ�File�ṹ */
	if (NULL == pFile)
	{
		/* ��ʧ��*/
		return;
	}

	/* u.u_arg[1] = pStatBuf */
	this->Stat1(pFile->f_inode, u.u_arg[1]); /* ��ȡ�ļ���Ϣ */
}

/* ������������ɻ�ȡ�ļ���Ϣ������� */
void FileManager::Stat()
{
	Inode *pInode;							/* �ļ�ָ�� */
	User &u = Kernel::Instance().GetUser(); /* ��ȡ�û����̶��� */

	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN); /* ��ȡ�ļ���Ϣ */
	if (NULL == pInode)
	{
		/* ��ʧ��*/
		return;
	}
	this->Stat1(pInode, u.u_arg[1]);  /* ��ȡ�ļ���Ϣ */
	this->m_InodeTable->IPut(pInode); /* �ͷ�Inode */
}

/* ������ϵͳ���õĹ������� */
void FileManager::Stat1(Inode *pInode, unsigned long statBuf)
{
	Buf *pBuf;													   /* ������ָ�� */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* ��ȡ������������� */

	pInode->IUpdate(Time::time);																									  /* ����Inode��ʱ��� */
	pBuf = bufMgr.Bread(pInode->i_dev, FileSystem::INODE_ZONE_START_SECTOR + pInode->i_number / FileSystem::INODE_NUMBER_PER_SECTOR); /* ��ȡ���Inode���ڵ����� */

	/* ��pָ�򻺴����б��Ϊinumber���Inode��ƫ��λ�� */
	unsigned char *p = pBuf->b_addr + (pInode->i_number % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
	Utility::DWordCopy((int *)p, (int *)statBuf, sizeof(DiskInode) / sizeof(int)); /* ����Inode��Ϣ���û��ռ� */

	bufMgr.Brelse(pBuf); /* �ͷŻ����� */
}

/*����������Read()ϵͳ���ô������*/
void FileManager::Read()
{
	/* ֱ�ӵ���Rdwr()�������� */
	this->Rdwr(File::FREAD);
}
/*����������Write()ϵͳ���ô������*/
void FileManager::Write()
{
	/* ֱ�ӵ���Rdwr()�������� */
	this->Rdwr(File::FWRITE);
}

/* ������������ɶ�дϵͳ���ù������ִ��� */
void FileManager::Rdwr(enum File::FileFlags mode)
{
	File *pFile;							/* �ļ�ָ�� */
	User &u = Kernel::Instance().GetUser(); /* ��ȡ�û����̶��� */

	/* ����Read()/Write()��ϵͳ���ò���fd��ȡ���ļ����ƿ�ṹ */
	pFile = u.u_ofiles.GetF(u.u_arg[0]); /* fd */
	if (NULL == pFile)
	{
		/* �����ڸô��ļ���GetF�Ѿ����ù������룬�������ﲻ��Ҫ�������� */
		/*	u.u_error = User::EBADF;	*/
		return;
	}

	/* ��д��ģʽ����ȷ */
	if ((pFile->f_flag & mode) == 0)
	{
		u.u_error = User::EACCES; /* �޷���Ȩ�� */
		return;
	}

	u.u_IOParam.m_Base = (unsigned char *)u.u_arg[1]; /* Ŀ�껺������ַ */
	u.u_IOParam.m_Count = u.u_arg[2];				  /* Ҫ���/д���ֽ��� */
	u.u_segflg = 0;									  /* User Space I/O�����������Ҫ�����ݶλ��û�ջ�� */

	/* �ܵ���д */
	if (pFile->f_flag & File::FPIPE)
	{
		/* ��д�ܵ�������Ҫ������ʣ���Ϊ�ܵ���ȫ˫���ģ���д�ǻ���ġ� */
		if (File::FREAD == mode)
		{
			/* ���ܵ� */
			this->ReadP(pFile);
		}
		else
		{
			/* д�ܵ� */
			this->WriteP(pFile);
		}
	}
	else
	/* ��ͨ�ļ���д �����д�����ļ������ļ�ʵʩ������ʣ���������ȣ�ÿ��ϵͳ���á�
	Ϊ��Inode����Ҫ��������������NFlock()��NFrele()��
	�ⲻ��V6����ơ�read��writeϵͳ���ö��ڴ�i�ڵ�������Ϊ�˸�ʵʩIO�Ľ����ṩһ�µ��ļ���ͼ��*/
	{
		pFile->f_inode->NFlock(); /* ���� */
		/* �����ļ���ʼ��λ�� */
		u.u_IOParam.m_Offset = pFile->f_offset;
		if (File::FREAD == mode)
		{
			/* ���ļ� */
			pFile->f_inode->ReadI();
		}
		else
		{
			/* д�ļ� */
			pFile->f_inode->WriteI();
		}

		/* ���ݶ�д�������ƶ��ļ���дƫ��ָ�� */
		pFile->f_offset += (u.u_arg[2] - u.u_IOParam.m_Count);
		pFile->f_inode->NFrele();
	}

	/* ����ʵ�ʶ�д���ֽ������޸Ĵ��ϵͳ���÷���ֵ�ĺ���ջ��Ԫ */
	u.u_ar0[User::EAX] = u.u_arg[2] - u.u_IOParam.m_Count;
}

/* ����������Pipe()�ܵ�����ϵͳ���ô������*/
void FileManager::Pipe()
{
	Inode *pInode;							/* ���ڴ����ܵ��ļ���Inode */
	File *pFileRead;						/* ���ܵ���File�ṹ */
	File *pFileWrite;						/* д�ܵ���File�ṹ */
	int fd[2];								/*Ϊ��д�ܵ���file�ṹ����Ĵ��ļ�������*/
	User &u = Kernel::Instance().GetUser(); /* ��ȡ��ǰ���̵�User�ṹ */

	/* ����һ��Inode���ڴ����ܵ��ļ� */
	pInode = this->m_FileSystem->IAlloc(DeviceManager::ROOTDEV);
	if (NULL == pInode)
	{
		/* ����ʧ�ܣ����ó����� */
		return;
	}

	/* ������ܵ���File�ṹ */
	pFileRead = this->m_OpenFileTable->FAlloc();
	if (NULL == pFileRead)
	{
		/* ����ʧ�ܣ������ܵ��ļ���Inode */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* ���ܵ��Ĵ��ļ���������ͨ��EAX��ô��ļ������� */
	fd[0] = u.u_ar0[User::EAX];

	/* ����д�ܵ���File�ṹ */
	pFileWrite = this->m_OpenFileTable->FAlloc();
	if (NULL == pFileWrite) /*������ʧ�ܣ������ܵ�������ص����д��ļ��ṹ*/
	{
		/* ����ʧ�ܣ������ܵ��ļ���Inode */
		pFileRead->f_count = 0;			  /* ���ܵ���File�ṹ�����ü������� */
		u.u_ofiles.SetF(fd[0], NULL);	  /* ���ܵ���File�ṹ�Ĵ��ļ����������� */
		this->m_InodeTable->IPut(pInode); /* �����ܵ��ļ���Inode */
		return;							  /* ���� */
	}

	/* д�ܵ��Ĵ��ļ������� */
	fd[1] = u.u_ar0[User::EAX]; /* ͨ��EAX��ô��ļ������� */

	/* Pipe(int* fd)�Ĳ�����u.u_arg[0]�У�������ɹ���2��fd���ظ��û�̬���� */
	int *pFdarr = (int *)u.u_arg[0]; /* ��д�ܵ��Ĵ��ļ����������� */
	pFdarr[0] = fd[0];				 /* ���ܵ��Ĵ��ļ������� */
	pFdarr[1] = fd[1];				 /* д�ܵ��Ĵ��ļ������� */

	/* ���ö���д�ܵ�File�ṹ������ ���Ժ�read��writeϵͳ������Ҫ�����ʶ*/
	pFileRead->f_flag = File::FREAD | File::FPIPE;	 /* ���ö��ܵ���File�ṹ�ı�ʶ */
	pFileRead->f_inode = pInode;					 /* ���ö��ܵ���File�ṹ��Inodeָ�� */
	pFileWrite->f_flag = File::FWRITE | File::FPIPE; /* ����д�ܵ���File�ṹ�ı�ʶ */
	pFileWrite->f_inode = pInode;					 /* ����д�ܵ���File�ṹ��Inodeָ�� */

	pInode->i_count = 2;						/* ���ùܵ��ļ���Inode�����ü���Ϊ2 */
	pInode->i_flag = Inode::IACC | Inode::IUPD; /* ���ùܵ��ļ���Inode�ı�ʶ */
	pInode->i_mode = Inode::IALLOC;				/* ���ùܵ��ļ���Inode��ģʽΪ�ܵ��ļ� */
}

/*���������ڹܵ�������*/
void FileManager::ReadP(File *pFile)
{
	Inode *pInode = pFile->f_inode;			/* ��ȡ�ܵ��ļ���Inode */
	User &u = Kernel::Instance().GetUser(); /* ��ȡ��ǰ���̵�User�ṹ */

loop:
	/* �Թܵ��ļ�������֤���� �������ڵ�V6�汾��ͨ�ļ��Ķ�дҲ��ȡ���ַǳ����ص�������ʽ*/
	pInode->Plock();

	/* �ܵ���û�����ݿɶ�ȡ ���ܵ��ļ���β����ʼд����i_size��дָ�롣*/
	if (pFile->f_offset == pInode->i_size)
	{
		if (pFile->f_offset != 0)
		{
			/* ���ܵ��ļ�ƫ�����͹ܵ��ļ���С����Ϊ0 */
			pFile->f_offset = 0;
			pInode->i_size = 0;

			/* �������IWRITE��־�����ʾ�н������ڵȴ�д�˹ܵ������Ա��뻽����Ӧ�Ľ��̡�*/
			if (pInode->i_mode & Inode::IWRITE)
			{
				pInode->i_mode &= (~Inode::IWRITE);											   /* ���IWRITE��־ */
				Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)(pInode + 1)); /* �������еȴ�д�ܵ��Ľ���,˯��ԭ��ΪpInode+1 */
			}
		}

		pInode->Prele(); /* �������Ļ���д�ܵ������޷��Թܵ�ʵʩ������ϵͳ���� */

		/* ����ܵ��Ķ��ߡ�д�����Ѿ���һ���رգ��򷵻� */
		if (pInode->i_count < 2)
		{
			return;
		}

		/* IREAD��־��ʾ�н��̵ȴ���Pipe */
		pInode->i_mode |= Inode::IREAD;
		/* ˯�ߵ�ǰ���̣��ȴ�д�ܵ����̻��� */
		u.u_procp->Sleep((unsigned long)(pInode + 2), ProcessManager::PPIPE); /* ˯��ԭ��ΪpInode+2 */
		goto loop;
	}

	/* �ܵ������пɶ�ȡ������ */
	u.u_IOParam.m_Offset = pFile->f_offset; /* ���ö�ȡ�ܵ���ƫ���� */
	pInode->ReadI();						/* ��ȡ�ܵ��е����� */
	pFile->f_offset = u.u_IOParam.m_Offset; /* ���¶�ȡ�ܵ���ƫ���� */
	pInode->Prele();						/* �����ܵ��ļ� */
}

/*���������ڹܵ�д����*/
void FileManager::WriteP(File *pFile)
{
	/* ��ȡ�ܵ��ļ���Inode */
	Inode *pInode = pFile->f_inode;
	/* ��ȡ��ǰ���̵�User�ṹ */
	User &u = Kernel::Instance().GetUser();
	/* ��ȡҪд��ܵ������ݳ��� */
	int count = u.u_IOParam.m_Count;

loop:
	/* �Թܵ��ļ�������֤���� �������ڵ�V6�汾��ͨ�ļ��Ķ�дҲ��ȡ���ַǳ����ص�������ʽ*/
	pInode->Plock();

	/* �������������д��ܵ����Թܵ�unlock������ */
	if (0 == count)
	{
		pInode->Prele();		 /* �����ܵ��ļ� */
		u.u_IOParam.m_Count = 0; /* ����д��ܵ������ݳ���Ϊ0 */
		return;					 /* ���� */
	}

	/* �ܵ����߽����ѹرն��ˡ����ź�SIGPIPE֪ͨӦ�ó��� */
	if (pInode->i_count < 2)
	{
		pInode->Prele();				   /* �����ܵ��ļ� */
		u.u_error = User::EPIPE;		   /* ���ô�����Ϊ�ܵ��ѹر� */
		u.u_procp->PSignal(User::SIGPIPE); /* ��ǰ���̷����ź�SIGPIPE */
		return;							   /* ���� */
	}

	/* ����Ѿ�����ܵ��ĵף�������ͬ����־��˯�ߵȴ� */
	if (Inode::PIPSIZ == pInode->i_size)
	{
		pInode->i_mode |= Inode::IWRITE;									  /* ����IWRITE��־ */
		pInode->Prele();													  /* �����ܵ��ļ� */
		u.u_procp->Sleep((unsigned long)(pInode + 1), ProcessManager::PPIPE); /* ˯��ԭ��ΪpInode+1 */
		goto loop;
	}

	/* ����д�����ݾ����ܶ��д��ܵ� */
	u.u_IOParam.m_Offset = pInode->i_size;											 /* ����д��ܵ���ƫ���� */
	u.u_IOParam.m_Count = Utility::Min(count, Inode::PIPSIZ - u.u_IOParam.m_Offset); /* ����д��ܵ������ݳ��� */
	count -= u.u_IOParam.m_Count;													 /* ���´�д��ܵ������ݳ��� */
	pInode->WriteI();																 /* ������д��ܵ� */
	pInode->Prele();																 /* �����ܵ��ļ� */

	/* ���Ѷ��ܵ����� */
	if (pInode->i_mode & Inode::IREAD)
	{
		pInode->i_mode &= (~Inode::IREAD);											   /* ���IREAD��־ */
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)(pInode + 2)); /* �������еȴ����ܵ��Ľ���,˯��ԭ��ΪpInode+2 */
	}
	goto loop;
}

/*�ú�������ȡ�����ļ�·�������Ӧ���ļ���Ŀ¼��inodeԪ��,�����������Inode*/
/* ����NULL��ʾĿ¼����ʧ�ܣ������Ǹ�ָ�룬ָ���ļ����ڴ��i�ڵ� ���������ڴ�i�ڵ�  */
Inode *FileManager::NameI(char (*func)(), enum DirectorySearchMode mode)
{
	Inode *pInode;												   /* ָ��ǰ������Ŀ¼��Inode */
	Buf *pBuf;													   /* ָ��ǰ������Ŀ¼�Ļ����� */
	char curchar;												   /* ��ǰ�ַ� */
	char *pChar;												   /* ָ��ǰ�ַ� */
	int freeEntryOffset;										   /* �Դ����ļ�ģʽ����Ŀ¼ʱ����¼����Ŀ¼���ƫ���� */
	User &u = Kernel::Instance().GetUser();						   /* ָ��ǰ���̵�User�ṹ */
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager(); /* ָ�򻺳��������� */

	/*
	 * �����·����'/'��ͷ�ģ��Ӹ�Ŀ¼��ʼ������
	 * ����ӽ��̵�ǰ����Ŀ¼��ʼ������
	 */
	pInode = u.u_cdir; /* ָ��ǰ���̵ĵ�ǰ����Ŀ¼ */
	if ('/' == (curchar = (*func)()))
	{
		pInode = this->rootDirInode; /* ָ���Ŀ¼ */
	}

	/* ����Inode�Ƿ����ڱ�ʹ�ã��Լ���֤������Ŀ¼���������и�Inode�����ͷ� */
	this->m_InodeTable->IGet(pInode->i_dev, pInode->i_number);

	/* �������////a//b ����·�� ����·���ȼ���/a/b */
	while ('/' == curchar)
	{
		curchar = (*func)(); /* ��ȡ��һ���ַ� */
	}
	/* �����ͼ���ĺ�ɾ����ǰĿ¼�ļ������ */
	if ('\0' == curchar && mode != FileManager::OPEN)
	{
		u.u_error = User::ENOENT; /* ���ô�����Ϊ�ļ������� */
		goto out;				  /* �ͷŵ�ǰ��������Ŀ¼�ļ�Inode�����˳� */
	}

	/* ���ѭ��ÿ�δ���pathname��һ��·������ */
	while (true)
	{
		/* ����������ͷŵ�ǰ��������Ŀ¼�ļ�Inode�����˳� */
		if (u.u_error != User::NOERROR)
		{
			break; /* goto out; */
		}

		/* ����·��������ϣ�������ӦInodeָ�롣Ŀ¼�����ɹ����ء� */
		if ('\0' == curchar)
		{
			return pInode; /* �����������Inode */
		}

		/* ���Ҫ���������Ĳ���Ŀ¼�ļ����ͷ����Inode��Դ���˳� */
		if ((pInode->i_mode & Inode::IFMT) != Inode::IFDIR)
		{
			u.u_error = User::ENOTDIR; /* ���ô�����Ϊ����Ŀ¼�ļ� */
			break;					   /* goto out; */
		}

		/* ����Ŀ¼����Ȩ�޼��,IEXEC��Ŀ¼�ļ��б�ʾ����Ȩ�� */
		if (this->Access(pInode, Inode::IEXEC))
		{
			u.u_error = User::EACCES; /* ���ô�����Ϊû������Ȩ�� */
			break;					  /* ���߱�Ŀ¼����Ȩ�ޣ�goto out; */
		}

		/*
		 * ��Pathname�е�ǰ׼������ƥ���·������������u.u_dbuf[]�У�
		 * ���ں�Ŀ¼����бȽϡ�
		 */
		pChar = &(u.u_dbuf[0]); /* ָ��u.u_dbuf[]���׵�ַ */
		while ('/' != curchar && '\0' != curchar && u.u_error == User::NOERROR)
		{
			if (pChar < &(u.u_dbuf[DirectoryEntry::DIRSIZ]))
			{
				/* ����ǰ�ַ�������u.u_dbuf[]�� */
				*pChar = curchar;
				pChar++;
			}
			curchar = (*func)(); /* ��ȡ��һ���ַ� */
		}
		/* ��u_dbufʣ��Ĳ������Ϊ'\0' */
		while (pChar < &(u.u_dbuf[DirectoryEntry::DIRSIZ]))
		{
			*pChar = '\0';
			pChar++;
		}

		/* �������////a//b ����·�� ����·���ȼ���/a/b */
		while ('/' == curchar)
		{
			curchar = (*func)(); /* ��ȡ��һ���ַ�,���Զ���/ */
		}

		if (u.u_error != User::NOERROR)
		{
			break; /* goto out; */
		}

		/* �ڲ�ѭ�����ֶ���u.u_dbuf[]�е�·���������������Ѱƥ���Ŀ¼�� */
		u.u_IOParam.m_Offset = 0;
		/* ����ΪĿ¼����� �����հ׵�Ŀ¼��*/
		u.u_IOParam.m_Count = pInode->i_size / (DirectoryEntry::DIRSIZ + 4);
		freeEntryOffset = 0; /* �Դ����ļ�ģʽ����Ŀ¼ʱ����¼����Ŀ¼���ƫ���� */
		pBuf = NULL;		 /* ָ�򻺳��� */

		while (true)
		{
			/* ��Ŀ¼���Ѿ�������� */
			if (0 == u.u_IOParam.m_Count)
			{
				if (NULL != pBuf)
				{
					/* �ͷŻ����� */
					bufMgr.Brelse(pBuf);
				}
				/* ����Ǵ������ļ� */
				if (FileManager::CREATE == mode && curchar == '\0')
				{
					/* �жϸ�Ŀ¼�Ƿ��д */
					if (this->Access(pInode, Inode::IWRITE))
					{
						u.u_error = User::EACCES; /* ���ô�����Ϊû��дȨ�� */
						goto out;				  /* Failed */
					}

					/* ����Ŀ¼Inodeָ�뱣���������Ժ�дĿ¼��WriteDir()�������õ� */
					u.u_pdir = pInode;

					if (freeEntryOffset) /* �˱�������˿���Ŀ¼��λ��Ŀ¼�ļ��е�ƫ���� */
					{
						/* ������Ŀ¼��ƫ��������u���У�дĿ¼��WriteDir()���õ� */
						u.u_IOParam.m_Offset = freeEntryOffset - (DirectoryEntry::DIRSIZ + 4);
					}
					else /*���⣺Ϊ��if��֧û����IUPD��־��  ������Ϊ�ļ��ĳ���û�б�ѽ*/
					{
						pInode->i_flag |= Inode::IUPD; /* ����IUPD��־����ʾĿ¼�ļ����޸� */
					}
					/* �ҵ�����д��Ŀ���Ŀ¼��λ�ã�NameI()�������� */
					return NULL;
				}

				/* Ŀ¼��������϶�û���ҵ�ƥ����ͷ����Inode��Դ�����Ƴ� */
				u.u_error = User::ENOENT; /* ���ô�����Ϊû���ҵ�ƥ���� */
				goto out;				  /* Failed */
			}

			/* �Ѷ���Ŀ¼�ļ��ĵ�ǰ�̿飬��Ҫ������һĿ¼�������̿� */
			if (0 == u.u_IOParam.m_Offset % Inode::BLOCK_SIZE)
			{
				if (NULL != pBuf)
				{
					/* �ͷŻ����� */
					bufMgr.Brelse(pBuf);
				}
				/* ����Ҫ���������̿�� */
				int phyBlkno = pInode->Bmap(u.u_IOParam.m_Offset / Inode::BLOCK_SIZE);
				/* ��ȡĿ¼�������̿� */
				pBuf = bufMgr.Bread(pInode->i_dev, phyBlkno);
			}

			/* û�ж��굱ǰĿ¼���̿飬���ȡ��һĿ¼����u.u_dent */
			int *src = (int *)(pBuf->b_addr + (u.u_IOParam.m_Offset % Inode::BLOCK_SIZE));
			/* ��Ŀ¼�����u.u_dent�� */
			Utility::DWordCopy(src, (int *)&u.u_dent, sizeof(DirectoryEntry) / sizeof(int));

			/* ����ƫ�����ͼ����� */
			u.u_IOParam.m_Offset += (DirectoryEntry::DIRSIZ + 4); /* 4�ֽ���Ŀ¼���е�ino */
			u.u_IOParam.m_Count--;								  /* Ŀ¼�������1 */

			/* ����ǿ���Ŀ¼���¼����λ��Ŀ¼�ļ���ƫ���� */
			if (0 == u.u_dent.m_ino)
			{
				if (0 == freeEntryOffset)
				{
					/* ��¼����Ŀ¼���ƫ���� */
					freeEntryOffset = u.u_IOParam.m_Offset;
				}
				/* ��������Ŀ¼������Ƚ���һĿ¼�� */
				continue;
			}

			int i;
			for (i = 0; i < DirectoryEntry::DIRSIZ; i++)
			{
				/* �Ƚ�pathname�е�ǰ·��������Ŀ¼���е��ļ����Ƿ���ͬ */
				if (u.u_dbuf[i] != u.u_dent.m_name[i])
				{
					break; /* ƥ����ĳһ�ַ�����������forѭ�� */
				}
			}

			if (i < DirectoryEntry::DIRSIZ)
			{
				/* ����Ҫ������Ŀ¼�����ƥ����һĿ¼�� */
				continue;
			}
			else
			{
				/* Ŀ¼��ƥ��ɹ����ص����While(true)ѭ�� */
				break;
			}
		}

		/*
		 * ���ڲ�Ŀ¼��ƥ��ѭ�������˴���˵��pathname��
		 * ��ǰ·������ƥ��ɹ��ˣ�����ƥ��pathname����һ·��
		 * ������ֱ������'\0'������
		 */
		if (NULL != pBuf)
		{
			/* �ͷŻ����� */
			bufMgr.Brelse(pBuf);
		}

		/* �����ɾ���������򷵻ظ�Ŀ¼Inode����Ҫɾ���ļ���Inode����u.u_dent.m_ino�� */
		if (FileManager::DELETE == mode && '\0' == curchar)
		{
			/* ����Ը�Ŀ¼û��д��Ȩ�� */
			if (this->Access(pInode, Inode::IWRITE))
			{
				u.u_error = User::EACCES; /* ���ô�����Ϊû��Ȩ�� */
				break;					  /* goto out; */
			}
			return pInode; /* ���ظ�Ŀ¼Inode */
		}

		/*
		 * ƥ��Ŀ¼��ɹ������ͷŵ�ǰĿ¼Inode������ƥ��ɹ���
		 * Ŀ¼��m_ino�ֶλ�ȡ��Ӧ��һ��Ŀ¼���ļ���Inode��
		 */
		short dev = pInode->i_dev;		  /* ��¼��ǰĿ¼Inode���豸�� */
		this->m_InodeTable->IPut(pInode); /* �ͷŵ�ǰĿ¼Inode */
		pInode = this->m_InodeTable->IGet(dev, u.u_dent.m_ino);
		/* �ص����While(true)ѭ��������ƥ��Pathname����һ·������ */

		if (NULL == pInode) /* ��ȡʧ�� */
		{
			return NULL;
		}
	}
/* �����While(true)ѭ�������˴���˵��ƥ��ʧ�� */
out:
	/* �ͷŵ�ǰĿ¼Inode */
	this->m_InodeTable->IPut(pInode);
	return NULL;
}

/* ���������ڻ��pathname�е���һ���ַ���pathname���û����̵�u.u_dirpָ��ָ����ַ����� */
char FileManager::NextChar()
{
	/* ���ں��л�ȡ�û����̵�User�ṹ */
	User &u = Kernel::Instance().GetUser();

	/* u.u_dirpָ��pathname�е��ַ� */
	return *u.u_dirp++;
}

/* ��creat���á�
 * Ϊ�´������ļ�д�µ�i�ڵ���µ�Ŀ¼��
 * ���ص�pInode�����������ڴ�i�ڵ㣬���е�i_count�� 1��
 *
 * �ڳ����������� WriteDir��������������Լ���Ŀ¼��д����Ŀ¼���޸ĸ�Ŀ¼�ļ���i�ڵ� ������д�ش��̡�
 *
 */
Inode *FileManager::MakNode(unsigned int mode)
{
	Inode *pInode;							/* ���ڱ����´����ļ���Inode */
	User &u = Kernel::Instance().GetUser(); /* ���ں��л�ȡ�û����̵�User�ṹ */

	/* ����һ������DiskInode������������ȫ����� */
	pInode = this->m_FileSystem->IAlloc(u.u_pdir->i_dev);
	if (NULL == pInode)
	{
		/* ����ʧ�� */
		return NULL;
	}

	pInode->i_flag |= (Inode::IACC | Inode::IUPD); /* ����IACC��IUPD��־ */
	pInode->i_mode = mode | Inode::IALLOC;		   /* �����ļ����ͺ�IALLOC��־ */
	pInode->i_nlink = 1;						   /* ����Ӳ������Ϊ1 */
	pInode->i_uid = u.u_uid;					   /* �����ļ������� */
	pInode->i_gid = u.u_gid;					   /* �����ļ������� */
	/* ��Ŀ¼��д��u.u_dent�����д��Ŀ¼�ļ� */
	this->WriteDir(pInode);
	return pInode; /* �����´����ļ���Inode */
}
/* ������������Ŀ¼��Ŀ¼�ļ�д��һ��Ŀ¼��*/
void FileManager::WriteDir(Inode *pInode)
{
	/* ���ں��л�ȡ�û����̵�User�ṹ */
	User &u = Kernel::Instance().GetUser();

	/* ����Ŀ¼����Inode��Ų��� */
	u.u_dent.m_ino = pInode->i_number;

	/* ����Ŀ¼����pathname�������� */
	for (int i = 0; i < DirectoryEntry::DIRSIZ; i++)
	{
		/* ��pathname����������Ŀ¼���� */
		u.u_dent.m_name[i] = u.u_dbuf[i];
	}

	u.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4; /* ���ö�д�ֽ���, 4��Inode��ŵ��ֽ��� */
	u.u_IOParam.m_Base = (unsigned char *)&u.u_dent;  /* ���ö�д������ */
	u.u_segflg = 1;									  /* ���öμĴ���ѡ���Ϊ�û����ݶ� */

	/* ��Ŀ¼��д�븸Ŀ¼�ļ� */
	u.u_pdir->WriteI();					/* ����Inode::WriteI() */
	this->m_InodeTable->IPut(u.u_pdir); /* �ͷŸ�Ŀ¼Inode */
}

/*�������������õ�ǰ����·��*/
void FileManager::SetCurDir(char *pathname)
{
	/* ���ں��л�ȡ�û����̵�User�ṹ */
	User &u = Kernel::Instance().GetUser();

	/* ·�����ǴӸ�Ŀ¼'/'��ʼ����������u.u_curdir������ϵ�ǰ·������ */
	if (pathname[0] != '/')
	{
		/* ���㵱ǰ����Ŀ¼�ĳ��� */
		int length = Utility::StringLength(u.u_curdir);
		if (u.u_curdir[length - 1] != '/')
		{
			/* �����ǰ����Ŀ¼������'/'��β�����ں������'/' */
			u.u_curdir[length] = '/';
			length++;
		}
		/* ����ǰ·������������u.u_curdir���� */
		Utility::StringCopy(pathname, u.u_curdir + length);
	}
	else /* ����ǴӸ�Ŀ¼'/'��ʼ����ȡ��ԭ�й���Ŀ¼ */
	{
		Utility::StringCopy(pathname, u.u_curdir);
	}
}

/*������Ϊ�����ļ���Ŀ¼������������Ȩ�ޣ���Ϊϵͳ���õĸ�������*/
/*
 * ����ֵ��0����ʾӵ�д��ļ���Ȩ�ޣ�1��ʾû������ķ���Ȩ�ޡ��ļ�δ�ܴ򿪵�ԭ���¼��u.u_error�����С�
 */
int FileManager::Access(Inode *pInode, unsigned int mode)
{
	/* ���ں��л�ȡ�û����̵�User�ṹ */
	User &u = Kernel::Instance().GetUser();

	/* ����д��Ȩ�ޣ���������ļ�ϵͳ�Ƿ���ֻ���� */
	if (Inode::IWRITE == mode)
	{
		/* �����ֻ���ļ�ϵͳ���򷵻�EROFS���� */
		if (this->m_FileSystem->GetFS(pInode->i_dev)->s_ronly != 0)
		{
			/* ��дֻ���ļ�ϵͳ */
			u.u_error = User::EROFS;
			return 1;
		}
	}
	/*
	 * ���ڳ����û�����д�κ��ļ����������
	 * ��Ҫִ��ĳ�ļ�ʱ��������i_mode�п�ִ�б�־
	 */
	if (u.u_uid == 0)
	{
		/* �����û� */
		if (Inode::IEXEC == mode && (pInode->i_mode & (Inode::IEXEC | (Inode::IEXEC >> 3) | (Inode::IEXEC >> 6))) == 0)
		{
			/* �����û�ִ���ļ�ʱ��������i_mode�п�ִ�б�־ */
			u.u_error = User::EACCES;
			return 1; /* Permission Check Failed! */
		}
		return 0; /* Permission Check Succeed! */
	}
	if (u.u_uid != pInode->i_uid)
	{
		/* ��������ļ������ߣ������ļ������� */
		mode = mode >> 3; /* ����ļ������� */
		if (u.u_gid != pInode->i_gid)
		{
			/* ��������ļ������飬���������û� */
			mode = mode >> 3;
		}
	}
	if ((pInode->i_mode & mode) != 0)
	{
		/* ����ļ���i_mode��������ķ���Ȩ�ޣ��򷵻�0 */
		return 0;
	}

	/* ����ļ���i_mode��û������ķ���Ȩ�ޣ��򷵻�1 */
	u.u_error = User::EACCES;
	return 1;
}

/*������Ϊ����ļ��Ƿ����ڵ�ǰ�û��ĺ���*/
Inode *FileManager::Owner()
{
	Inode *pInode;							/* ���ڴ���ļ�Inodeָ�� */
	User &u = Kernel::Instance().GetUser(); /* ���ں��л�ȡ�û����̵�User�ṹ */

	if ((pInode = this->NameI(NextChar, FileManager::OPEN)) == NULL)
	{
		/* �ļ������� */
		return NULL;
	}

	if (u.u_uid == pInode->i_uid || u.SUser())
	{
		/* ������ļ������߻򳬼��û����򷵻��ļ�Inodeָ�� */
		return pInode;
	}

	/* ��������ļ�������Ҳ���ǳ����û������ͷ��ļ�Inode������NULL */
	this->m_InodeTable->IPut(pInode);
	/* �ļ������ڵ�ǰ�û� */
	return NULL;
}

/* ���������ڸı��ļ�����ģʽ */
void FileManager::ChMod()
{
	Inode *pInode;							/* ���ڴ���ļ�Inodeָ�� */
	User &u = Kernel::Instance().GetUser(); /* ���ں��л�ȡ�û����̵�User�ṹ */
	unsigned int mode = u.u_arg[1];			/* ��ϵͳ���ò����л�ȡ�µķ���ģʽ */

	if ((pInode = this->Owner()) == NULL)
	{
		/* �ļ������ڵ�ǰ�û� */
		return;
	}
	/* clear i_mode�ֶ��е�ISGID, ISUID, ISTVX�Լ�rwxrwxrwx��12���� */
	pInode->i_mode &= (~0xFFF);
	/* ����ϵͳ���õĲ�����������i_mode�ֶ� */
	pInode->i_mode |= (mode & 0xFFF);
	pInode->i_flag |= Inode::IUPD;

	/* �ͷ��ļ�Inode */
	this->m_InodeTable->IPut(pInode);
	return;
}

/* ���������ڸı��ļ��������� */
void FileManager::ChOwn()
{
	Inode *pInode;							/* ���ڴ���ļ�Inodeָ�� */
	User &u = Kernel::Instance().GetUser(); /* ���ں��л�ȡ�û����̵�User�ṹ */
	short uid = u.u_arg[1];					/* ��ϵͳ���ò����л�ȡ�µ��ļ������� */
	short gid = u.u_arg[2];					/* ��ϵͳ���ò����л�ȡ�µ��ļ������� */

	/* ���ǳ����û����߲����ļ����򷵻� */
	if (!u.SUser() || (pInode = this->Owner()) == NULL)
	{
		/* �ļ������ڵ�ǰ�û� */
		return;
	}
	pInode->i_uid = uid;		   /* �����ļ������� */
	pInode->i_gid = gid;		   /* �����ļ������� */
	pInode->i_flag |= Inode::IUPD; /* �����ļ�Inode���޸ı�־ */

	this->m_InodeTable->IPut(pInode); /* �ͷ��ļ�Inode */
}

/* ���������ڸı䵱ǰ����Ŀ¼ */
void FileManager::ChDir()
{
	Inode *pInode;							/* ���ڴ���ļ�Inodeָ�� */
	User &u = Kernel::Instance().GetUser(); /* ���ں��л�ȡ�û����̵�User�ṹ */

	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN); /* ���ļ� */
	if (NULL == pInode)
	{
		/* �ļ������� */
		return;
	}
	/* ���������ļ�����Ŀ¼�ļ� */
	if ((pInode->i_mode & Inode::IFMT) != Inode::IFDIR)
	{
		/* ���ô����ΪENOTDIR */
		u.u_error = User::ENOTDIR;
		/* �ͷ��ļ�Inode */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* �ж�����ִ��Ȩ�� */
	if (this->Access(pInode, Inode::IEXEC))
	{
		/* �ͷ��ļ�Inode */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* �ͷŵ�ǰĿ¼��Inode */
	this->m_InodeTable->IPut(u.u_cdir);
	/* ���õ�ǰĿ¼��Inode */
	u.u_cdir = pInode;
	/* �ͷŵ�ǰinode�� */
	pInode->Prele();
	/* ���õ�ǰ����Ŀ¼ */
	this->SetCurDir((char *)u.u_arg[0] /* pathname */);
}

/* ���������ڴ����ļ����������� */
void FileManager::Link()
{
	Inode *pInode;							/* ���ڴ���ļ�Inodeָ�� */
	Inode *pNewInode;						/* ���ڴ�����ļ�Inodeָ�� */
	User &u = Kernel::Instance().GetUser(); /* ���ں��л�ȡ�û����̵�User�ṹ */
	/* ���ļ�����ȡ�ļ�Inodeָ�� */
	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN);
	/* ���ļ�ʧ�� */
	if (NULL == pInode)
	{
		/* �ļ������� */
		return;
	}
	/* ���ӵ������Ѿ���� */
	if (pInode->i_nlink >= 255)
	{
		/* ���ô����ΪEMLINK */
		u.u_error = User::EMLINK;
		/* �����ͷ���Դ���˳� */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* ��Ŀ¼�ļ�������ֻ���ɳ����û����� */
	if ((pInode->i_mode & Inode::IFMT) == Inode::IFDIR && !u.SUser())
	{
		/* �����ͷ���Դ���˳� */
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* �����ִ��ļ�Inode,�Ա����������������ļ�ʱ�������� */
	pInode->i_flag &= (~Inode::ILOCK);
	/* ָ��Ҫ��������·��newPathname */
	u.u_dirp = (char *)u.u_arg[1];
	pNewInode = this->NameI(FileManager::NextChar, FileManager::CREATE);
	/* ����ļ��Ѵ��� */
	if (NULL != pNewInode)
	{
		/* ���ô����ΪEEXIST */
		u.u_error = User::EEXIST;
		/* �����ͷ���Դ���˳� */
		this->m_InodeTable->IPut(pNewInode);
	}
	if (User::NOERROR != u.u_error)
	{
		/* �����ͷ���Դ���˳� */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* ���Ŀ¼����ļ��Ƿ���ͬһ���豸�� */
	if (u.u_pdir->i_dev != pInode->i_dev)
	{
		/* �����ͷ���Դ���˳� */
		this->m_InodeTable->IPut(u.u_pdir);
		/* ���ô����ΪEXDEV */
		u.u_error = User::EXDEV;
		/* �����ͷ���Դ���˳� */
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* д��Ŀ¼�� */
	this->WriteDir(pInode);
	/* ����Ŀ���ļ���link�� */
	pInode->i_nlink++;
	/* �����ļ�Inode���޸ı�־ */
	pInode->i_flag |= Inode::IUPD;
	/* �ͷ�Ŀ¼�ļ�Inode */
	this->m_InodeTable->IPut(pInode);
}

/* ����������ɾ���ļ����������� */
void FileManager::UnLink()
{
	Inode *pInode;							/* ���ڴ���ļ�Inodeָ�� */
	Inode *pDeleteInode;					/* ���ڴ��Ҫɾ�����ļ�Inodeָ�� */
	User &u = Kernel::Instance().GetUser(); /* ���ں��л�ȡ�û����̵�User�ṹ */

	pDeleteInode = this->NameI(FileManager::NextChar, FileManager::DELETE); /* ���ļ�����ȡ�ļ�Inodeָ�� */
	if (NULL == pDeleteInode)
	{
		/* �ļ������� */
		return;
	}
	pDeleteInode->Prele(); /* �����ļ�Inode */

	pInode = this->m_InodeTable->IGet(pDeleteInode->i_dev, u.u_dent.m_ino); /* ��ȡĿ¼�ļ�Inode */
	if (NULL == pInode)
	{
		Utility::Panic("unlink -- iget"); /* ��ȡʧ�ܣ��ں˳��� */
	}
	/* ֻ��root����unlinkĿ¼�ļ� */
	if ((pInode->i_mode & Inode::IFMT) == Inode::IFDIR && !u.SUser())
	{
		/* �����ͷ���Դ���˳� */
		this->m_InodeTable->IPut(pDeleteInode);
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* д��������Ŀ¼�� */
	u.u_IOParam.m_Offset -= (DirectoryEntry::DIRSIZ + 4); /* ��������ƫ���� */
	u.u_IOParam.m_Base = (unsigned char *)&u.u_dent;	  /* �������û���ַ */
	u.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;	  /* ���������ֽ��� */

	u.u_dent.m_ino = 0;		/* ����Ŀ¼���е�inode�� */
	pDeleteInode->WriteI(); /* д��Ŀ¼�� */

	/* �޸�inode�� */
	pInode->i_nlink--;			   /* ��������1 */
	pInode->i_flag |= Inode::IUPD; /* �����ļ�Inode���޸ı�־ */

	this->m_InodeTable->IPut(pDeleteInode); /* �ͷ��ļ�Inode */
	this->m_InodeTable->IPut(pInode);		/* �ͷ�Ŀ¼�ļ�Inode */
}

/* ���ڽ��������豸�ļ���ϵͳ���� */
void FileManager::MkNod()
{
	Inode *pInode;							/* ���ڴ���ļ�Inodeָ�� */
	User &u = Kernel::Instance().GetUser(); /* ���ں��л�ȡ�û����̵�User�ṹ */

	/* ���uid�Ƿ���root����ϵͳ����ֻ��uid==rootʱ�ſɱ����� */
	if (u.SUser())
	{
		/* ָ��Ҫ��������·��newPathname */
		pInode = this->NameI(FileManager::NextChar, FileManager::CREATE);
		/* Ҫ�������ļ��Ѿ�����,���ﲢ����ȥ���Ǵ��ļ� */
		if (pInode != NULL)
		{
			u.u_error = User::EEXIST;		  /* ���ô����ΪEEXIST */
			this->m_InodeTable->IPut(pInode); /* �ͷ��ļ�Inode */
			return;							  /* �˳� */
		}
	}
	else
	{
		/* ��root�û�ִ��mknod()ϵͳ���÷���User::EPERM */
		u.u_error = User::EPERM;
		return;
	}
	/* û��ͨ��SUser()�ļ�� */
	if (User::NOERROR != u.u_error)
	{
		return; /* û����Ҫ�ͷŵ���Դ��ֱ���˳� */
	}
	/* ��ȡ�ļ�Inode */
	pInode = this->MakNode(u.u_arg[1]);
	if (NULL == pInode)
	{
		/* ��ȡʧ�ܣ��ں˳��� */
		return;
	}
	/* ���������豸�ļ� */
	if ((pInode->i_mode & (Inode::IFBLK | Inode::IFCHR)) != 0)
	{
		/* �����������豸�ļ�������i_addr[0]��ŵ����豸�� */
		pInode->i_addr[0] = u.u_arg[2];
	}
	/* �ͷ��ļ�Inode */
	this->m_InodeTable->IPut(pInode);
}
/*==========================class DirectoryEntry===============================*/
/* ���ڳ�ʼ��Ŀ¼�� */
DirectoryEntry::DirectoryEntry()
{
	/* ��ʼ��Ŀ¼�� */
	this->m_ino = 0;
	/* ��ʼ��Ŀ¼������ */
	this->m_name[0] = '\0';
}

DirectoryEntry::~DirectoryEntry()
{
	// nothing to do here
}
