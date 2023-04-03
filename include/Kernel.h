/*
 * @Description: 
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-03-22 21:00:31
 */
#ifndef KERNEL_H
#define KERNEL_H

#include "PageManager.h"
#include "ProcessManager.h"
#include "KernelAllocator.h"
#include "User.h"
#include "BufferManager.h"
#include "DeviceManager.h"
#include "FileManager.h"
#include "FileSystem.h"
#include "SwapperManager.h"

/*
 * Kernel�����ڷ�װ�����ں���ص�ȫ����ʵ������
 * ����PageManager, ProcessManager�ȡ�
 * 
 * Kernel�����ڴ���Ϊ����ģʽ����֤�ں��з�װ���ں�
 * ģ��Ķ���ֻ��һ��������
 */
class Kernel
{
public:
	static const unsigned long USER_ADDRESS = 0x400000 - 0x1000 + 0xc0000000;	/* 0xC03FF000  user�ṹ����ʼ���ַ*/
	static const unsigned long USER_PAGE_INDEX = 1023;		/* USER_ADDRESS��Ӧҳ������PageTable�е����� */

public:
	Kernel();	//���캯��
	~Kernel();	//��������
	static Kernel& Instance();	//����Kernelʵ��
	void Initialize();		/* �ú�����ɳ�ʼ���ں˴󲿷����ݽṹ�ĳ�ʼ�� */

	KernelPageManager& GetKernelPageManager();	//����m_KernelPageManager
	UserPageManager& GetUserPageManager();	//����m_UserPageManager
	ProcessManager& GetProcessManager();	//����m_ProcessManager
	KernelAllocator& GetKernelAllocator();	//����m_ProcessManager
	SwapperManager& GetSwapperManager();	//����m_SwapperManager
	BufferManager& GetBufferManager();	//����m_BufferManager
	DeviceManager& GetDeviceManager();	//����m_DeviceManager
	FileSystem& GetFileSystem();	//����m_FileSystem
	FileManager& GetFileManager();	//����m_FileManager
	User& GetUser();		/* ��ȡ��ǰ���̵�User�ṹ */

private:
	void InitMemory();	//�ڴ��ʼ��
	void InitProcess();	//���̳�ʼ��
	void InitBuffer();	//buffer��ʼ��
	void InitFileSystem();	//�ļ�ϵͳ��ʼ��

private:
	static Kernel instance;		/* Kernel������ʵ�� */

	KernelPageManager* m_KernelPageManager;	//�ں�ҳ�����
	UserPageManager* m_UserPageManager;	//�û�ҳ�����
	ProcessManager* m_ProcessManager;	//���̹���
	KernelAllocator* m_KernelAllocator;	//�ں��ڴ����
	SwapperManager* m_SwapperManager;	//����������
	BufferManager* m_BufferManager;	//����������
	DeviceManager* m_DeviceManager;	//�豸����
	FileSystem* m_FileSystem;	//�ļ�ϵͳ
	FileManager* m_FileManager;	//�ļ�����
};

#endif
