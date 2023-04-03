/*
 * @Description:
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:41:30
 */
#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include "Buf.h"
#include "Utility.h"

/* ���豸��devtab���� */
class Devtab
{
public:
	Devtab();  // ���캯��
	~Devtab(); // ��������

public:
	int d_active; // ��������ִ��IO����ʱ��d_active��1
	int d_errcnt; // ��ǰ����IO��������������ִ���ʱ���ñ���������10ʱϵͳ�Ż����
	Buf *b_forw;  // �豸������еĶ���
	Buf *b_back;  // �豸������еĶ�β
	Buf *d_actf;  // IO ������еĶ���
	Buf *d_actl;  // IO ������еĶ�β
};

/*
 * ���豸���࣬������豸���Ӵ˻���̳С�
 * ������override�����е�Open(), Close(), Strategy()������
 * ʵ�ֶԸ��п��豸��ͬ�Ĳ����߼����Դ����Unix V6��
 * ԭ���豸���ر�(bdevsw)�Ĺ��ܡ�
 */
class BlockDevice
{
public:
	BlockDevice();			  // ���캯��
	BlockDevice(Devtab *tab); // ʹ�ÿ��豸��tab��ʼ��
	virtual ~BlockDevice();	  // ��������
	/*
	 * ����Ϊ�麯���������������overrideʵ���豸
	 * �ض���������������£������к�����Ӧ�����õ���
	 */
	virtual int Open(short dev, int mode);	// ����
	virtual int Close(short dev, int mode); // ����
	virtual int Strategy(Buf *bp);			// ����
	virtual void Start();					// ����

public:
	Devtab *d_tab; /* ָ����豸���ָ�� */
};

/* ATA�����豸�����ࡣ�ӿ��豸����BlockDevice�̳ж����� */
class ATABlockDevice : public BlockDevice
{
public:
	static int NSECTOR; /* ATA���������� */

public:
	ATABlockDevice(Devtab *tab);
	virtual ~ATABlockDevice();
	/*
	 * Override����BlockDevice�е��麯����ʵ��
	 * ������ATABlockDevice�ض����豸�����߼���
	 */
	int Open(short dev, int mode);	// Ϊ�գ�����Բ����ת��Ӳ�̲���Ҫ�򿪹ر�
	int Close(short dev, int mode); // Ϊ�գ�����Բ����ת��Ӳ�̲���Ҫ�򿪹ر�
	int Strategy(Buf *bp);			// ά�� FIFS ��������С�������Ŷ�β���������أ�����Ϊ�գ�����Start �����������̡�
	/*
	 * �������豸ִ��I/O���󡣴˺��������������
	 * I/O������У����Ӳ�̿������������в�����
	 */
	void Start();
};

#endif
