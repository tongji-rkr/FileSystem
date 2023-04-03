/*
 * @Description: this file defines the class File
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */
#include "File.h"
#include "Utility.h" //for NULL
#include "Kernel.h"

/*==============================class File===================================*/
File::File()
{
	this->f_count = 0;
	this->f_flag = 0;
	this->f_offset = 0;
	this->f_inode = NULL;
}

File::~File()
{
	// nothing to do here
}

/*==============================class OpenFiles===================================*/
OpenFiles::OpenFiles()
{
}

OpenFiles::~OpenFiles()
{
}

/*���������ڷ������ô��ļ�����*/
int OpenFiles::AllocFreeSlot()
{
	int i;									/* ����ѭ������ */
	User &u = Kernel::Instance().GetUser(); /*��õ�ǰ�û�User�ṹ*/

	for (i = 0; i < OpenFiles::NOFILES; i++)
	{
		/* ���̴��ļ������������ҵ�������򷵻�֮ */
		if (this->ProcessOpenFileTable[i] == NULL)
		{
			/* ���ú���ջ�ֳ��������е�EAX�Ĵ�����ֵ����ϵͳ���÷���ֵ */
			u.u_ar0[User::EAX] = i; /*��������ͨ��User�ṹ��EAX�ķ�ʽ����*/
			return i;				/*�ɹ����䣬���ر�����*/
		}
	}

	u.u_ar0[User::EAX] = -1;  /* Open1����Ҫһ����־�������ļ��ṹ����ʧ��ʱ�����Ի���ϵͳ��Դ*/
	u.u_error = User::EMFILE; /* ���ô���� */
	return -1;				  /* ����ʧ�ܣ�����-1 */
}

/* ���������ڿ�¡���ļ����ʵ���ϸ���û�б����ù� */
int OpenFiles::Clone(int fd)
{
	return 0;
}

/* ����������ͨ�����ļ������±�������ļ���дָ�� */
File *OpenFiles::GetF(int fd)
{
	File *pFile;							/* ���ڴ�Ŵ��ļ������ָ�� */
	User &u = Kernel::Instance().GetUser(); /*��õ�ǰ�û�User�ṹ*/

	/* ������ļ���������ֵ�����˷�Χ */
	if (fd < 0 || fd >= OpenFiles::NOFILES)
	{
		u.u_error = User::EBADF; /* ���ô���� */
		return NULL;			 /* ��ʧ�ܷ���NULL */
	}

	/* ��ý��̴��ļ�������������Ӧ��File�ṹ*/
	pFile = this->ProcessOpenFileTable[fd];
	if (pFile == NULL)
	{
		/* ������̴��ļ�������������Ӧ��File�ṹΪ�գ������ô���� */
		u.u_error = User::EBADF;
	}

	return pFile; /* ��ʹpFile==NULLҲ���������ɵ���GetF�ĺ������жϷ���ֵ */
}

/*�������������ý��̴��ļ����������ж�Ӧ�������Ӧ��File�ṹ*/
void OpenFiles::SetF(int fd, File *pFile)
{
	/* ������ļ���������ֵ�����˷�Χ */
	if (fd < 0 || fd >= OpenFiles::NOFILES)
	{
		return; /* ��ʧ�ܷ��� */
	}
	/* ���̴��ļ�������ָ��ϵͳ���ļ�������Ӧ��File�ṹ */
	this->ProcessOpenFileTable[fd] = pFile;
}

/*==============================class IOParameter===================================*/
/* �ļ���������Ĺ��캯�� */
IOParameter::IOParameter()
{
	this->m_Base = 0;	/* ��ǰ����д�û�Ŀ��������׵�ַ */
	this->m_Count = 0;	/* ��ǰ��ʣ��Ķ���д�ֽ����� */
	this->m_Offset = 0; /* ��ǰ����д�ļ����ֽ�ƫ���� */
}

/* �ļ������������������ */
IOParameter::~IOParameter()
{
	// nothing to do here
}
