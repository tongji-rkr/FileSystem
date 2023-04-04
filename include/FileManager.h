#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "FileSystem.h"
#include "OpenFileManager.h"
#include "File.h"

/* 
 * 文件管理类(FileManager)
 * 封装了文件系统的各种系统调用在核心态下处理过程，
 * 如对文件的Open()、Close()、Read()、Write()等等
 * 封装了对文件系统访问的具体细节。
 */
class FileManager
{
public:
	/* 目录搜索模式，用于NameI()函数 */
	enum DirectorySearchMode
	{
		OPEN = 0,		/* 以打开文件方式搜索目录 */
		CREATE = 1,		/* 以新建文件方式搜索目录 */
		DELETE = 2		/* 以删除文件方式搜索目录 */
	};

	/* Functions */
public:
	/* Constructors */
	FileManager();
	/* Destructors */
	~FileManager();


	/* 
	 * @comment 初始化对全局对象的引用
	 */
	void Initialize();

	/* 
	 * @comment Open()系统调用处理过程
	 */
	void Open();

	/* 
	 * @comment Creat()系统调用处理过程
	 */
	void Creat();

	/* 
	 * @comment Open()、Creat()系统调用的公共部分
	 */
	void Open1(Inode* pInode, int mode, int trf);

	/* 
	 * @comment Close()系统调用处理过程
	 */
	void Close();

	/* 
	 * @comment Seek()系统调用处理过程
	 */
	void Seek();

	/* 
	 * @comment Read()系统调用处理过程
	 */
	void Read();

	/* 
	 * @comment Write()系统调用处理过程
	 */
	void Write();

	/* 
	 * @comment 读写系统调用公共部分代码
	 */
	void Rdwr(enum File::FileFlags mode);

	/* 
	 * @comment 目录搜索，将路径转化为相应的Inode，
	 * 返回上锁后的Inode
	 */
	Inode* NameI(char (*func)(), enum DirectorySearchMode mode);

	/* 
	 * @comment 获取路径中的下一个字符
	 */
	static char NextChar();

	/* 
	 * @comment 被Creat()系统调用使用，用于为创建新文件分配内核资源
	 */
	Inode* MakNode(unsigned int mode);

	/* 
	 * @comment 向父目录的目录文件写入一个目录项
	 */
	void WriteDir(Inode* pInode);

	/*
	 * @comment 设置当前工作路径
	 */
	void SetCurDir(char* pathname);

	/* 
	 * @comment 检查对文件或目录的搜索、访问权限，作为系统调用的辅助函数
	 */
	int Access(Inode* pInode, unsigned int mode);

	/* 
	 * @comment 创建文件的异名引用
	 */
	void Link();

	/* 
	 * @comment 取消文件的异名引用
	 */
	void UnLink();

	/* 
	 * @comment 用于建立特殊设备文件的系统调用
	 */
	void MkNod();

	/* 
	 * @comment 改变当前工作路径
	 */
	void ChDir();
	
public:
	/* 根目录内存Inode */
	Inode* rootDirInode;

	/* 对全局对象g_FileSystem的引用，该对象负责管理文件系统存储资源 */
	FileSystem* m_FileSystem;
	
	/* 对全局对象g_InodeTable的引用，该对象负责内存Inode表的管理 */
	InodeTable* m_InodeTable;

	/* 对全局对象g_OpenFileTable的引用，该对象负责打开文件表项的管理 */
	OpenFileTable* m_OpenFileTable;
};


class DirectoryEntry
{
	/* static members */
public:
	static const int DIRSIZ = 28;	/* 目录项中路径部分的最大字符串长度 */

	/* Functions */
public:
	/* Constructors */
	DirectoryEntry();
	/* Destructors */
	~DirectoryEntry();

	/* Members */
public:
	int m_ino;		/* 目录项中Inode编号部分 */
	char m_name[DIRSIZ];	/* 目录项中路径名部分 */
};

#endif
