/*
 * @Description:
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:58
 */
#ifndef CHAR_DEVICE_H
#define CHAR_DEVICE_H

#include "TTy.h"

// �ַ��豸��
class CharDevice
{
public:
	CharDevice();		   // ���캯��
	virtual ~CharDevice(); // ��������
	/*
	 * ����Ϊ�麯���������������overrideʵ���豸
	 * �ض���������������£������к�����Ӧ�����õ���
	 */
	virtual void Open(short dev, int mode);	  // �����в�������
	virtual void Close(short dev, int mode);  // �����в�������
	virtual void Read(short dev);			  // �����в�������
	virtual void Write(short dev);			  // �����в�������
	virtual void SgTTy(short dev, TTy *pTTy); // �����в�������

public:
	TTy *m_TTy; /* ָ���ַ��豸TTy�ṹ��ָ�� */
};

class ConsoleDevice : public CharDevice
{
public:
	ConsoleDevice();		  // ���캯��
	virtual ~ConsoleDevice(); // ��������
	/*
	 * Override����CharDevice�е��麯����ʵ��
	 * ������ConsoleDevice�ض����豸�����߼���
	 */
	void Open(short dev, int mode);	  // ���豸
	void Close(short dev, int mode);  // �ر��豸
	void Read(short dev);			  // ����豸��tty��ִ��tty�豸ͨ�ö�����
	void Write(short dev);			  // ����豸��tty��ִ��tty�豸ͨ��д����
	void SgTTy(short dev, TTy *pTTy); // Ϊ��
};

#endif
