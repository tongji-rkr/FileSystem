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

/*本函数用于分配闲置打开文件表项*/
int OpenFiles::AllocFreeSlot()
{
	int i;									/* 用于循环计数 */
	User &u = Kernel::Instance().GetUser(); /*获得当前用户User结构*/

	for (i = 0; i < OpenFiles::NOFILES; i++)
	{
		/* 进程打开文件描述符表中找到空闲项，则返回之 */
		if (this->ProcessOpenFileTable[i] == NULL)
		{
			/* 设置核心栈现场保护区中的EAX寄存器的值，即系统调用返回值 */
			u.u_ar0[User::EAX] = i; /*将表项编号通过User结构中EAX的方式传递*/
			return i;				/*成功分配，返回表项编号*/
		}
	}

	u.u_ar0[User::EAX] = -1;  /* Open1，需要一个标志。当打开文件结构创建失败时，可以回收系统资源*/
	u.u_error = User::EMFILE; /* 设置错误号 */
	return -1;				  /* 分配失败，返回-1 */
}

/* 本函数用于克隆打开文件表项，实际上根本没有被调用过 */
int OpenFiles::Clone(int fd)
{
	return 0;
}

/* 本函数用于通过打开文件表项下标来获得文件读写指针 */
File *OpenFiles::GetF(int fd)
{
	File *pFile;							/* 用于存放打开文件表项的指针 */
	User &u = Kernel::Instance().GetUser(); /*获得当前用户User结构*/

	/* 如果打开文件描述符的值超出了范围 */
	if (fd < 0 || fd >= OpenFiles::NOFILES)
	{
		u.u_error = User::EBADF; /* 设置错误号 */
		return NULL;			 /* 打开失败返回NULL */
	}

	/* 获得进程打开文件描述符表中相应的File结构*/
	pFile = this->ProcessOpenFileTable[fd];
	if (pFile == NULL)
	{
		/* 如果进程打开文件描述符表中相应的File结构为空，则设置错误号 */
		u.u_error = User::EBADF;
	}

	return pFile; /* 即使pFile==NULL也返回它，由调用GetF的函数来判断返回值 */
}

/*本函数用于设置进程打开文件描述符表中对应表项的相应的File结构*/
void OpenFiles::SetF(int fd, File *pFile)
{
	/* 如果打开文件描述符的值超出了范围 */
	if (fd < 0 || fd >= OpenFiles::NOFILES)
	{
		return; /* 打开失败返回 */
	}
	/* 进程打开文件描述符指向系统打开文件表中相应的File结构 */
	this->ProcessOpenFileTable[fd] = pFile;
}

/*==============================class IOParameter===================================*/
/* 文件参数对象的构造函数 */
IOParameter::IOParameter()
{
	this->m_Base = 0;	/* 当前读、写用户目标区域的首地址 */
	this->m_Count = 0;	/* 当前还剩余的读、写字节数量 */
	this->m_Offset = 0; /* 当前读、写文件的字节偏移量 */
}

/* 文件参数对象的析构函数 */
IOParameter::~IOParameter()
{
	// nothing to do here
}
