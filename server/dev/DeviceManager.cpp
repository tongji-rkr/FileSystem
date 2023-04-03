/*
 * @Description: this file defines the class DeviceManager
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:41:15
 */
#include "DeviceManager.h"

extern ATABlockDevice g_ATADevice;	  // ATA�����豸
extern ConsoleDevice g_ConsoleDevice; // ����̨�豸

// ���캯��
DeviceManager::DeviceManager()
{
}
// ��������
DeviceManager::~DeviceManager()
{
}
// ��ʼ��
void DeviceManager::Initialize()
{
	this->bdevsw[0] = &g_ATADevice; // ��һ�����豸�����涨���ATA�����豸
	this->nblkdev = 1;				// Ŀǰ���豸����Ϊ1

	this->cdevsw[0] = &g_ConsoleDevice; // ��һ���ַ��豸�����涨��Ŀ���̨�豸
	this->nchrdev = 1;					// Ŀǰ�ַ��豸����Ϊ1
}

// ��ȡ���豸������
int DeviceManager::GetNBlkDev()
{
	return this->nblkdev;
}

// �������豸�Ż�ȡָ��ÿ��豸��ָ��
BlockDevice &DeviceManager::GetBlockDevice(short major)
{
	// ��֤���豸�źϷ�
	if (major >= this->nblkdev || major < 0)
	{
		Utility::Panic("Block Device Doesn't Exist!"); // ���豸�Ų��Ϸ�ֱ�ӱ���
	}
	return *(this->bdevsw[major]); // ����major��Ӧ�Ŀ��豸
}

// ����ַ��豸������
int DeviceManager::GetNChrDev()
{
	return this->nchrdev;
}

// �������豸�Ż�ȡָ����ַ��豸��ָ��
CharDevice &DeviceManager::GetCharDevice(short major)
{
	// ��֤���豸�źϷ�
	if (major >= this->nchrdev || major < 0)
	{
		Utility::Panic("Char Device Doesn't Exist!"); // ���豸�Ų��Ϸ�ֱ�ӱ���
	}
	return *(this->cdevsw[major]); // ����mahor��Ӧ���ַ��豸
}
