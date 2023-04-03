/*
 * @Description: this file defines the class CharDevice
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */
#include "CharDevice.h"
#include "Utility.h"
#include "Kernel.h"

/*==============================class CharDevice===============================*/
CharDevice::CharDevice() // ���캯��
{
	this->m_TTy = NULL; // ָ���ַ��豸TTY�ṹ��ָ���ÿ�
}

CharDevice::~CharDevice() // ��������
{
	// nothing to do here
}

void CharDevice::Open(short dev, int mode) // ��Ӧ��ִ��
{
	Utility::Panic("ERROR! Base Class: CharDevice::Open()!"); // ����
}

void CharDevice::Close(short dev, int mode) // ��Ӧ��ִ��
{
	Utility::Panic("ERROR! Base Class: CharDevice::Close()!"); // ����
}

void CharDevice::Read(short dev) // ��Ӧ��ִ��
{
	Utility::Panic("ERROR! Base Class: CharDevice::Read()!"); // ����
}

void CharDevice::Write(short dev) // ��Ӧ��ִ��
{
	Utility::Panic("ERROR! Base Class: CharDevice::Write()!"); // ����
}

void CharDevice::SgTTy(short dev, TTy *pTTy) // ��Ӧ��ִ��
{
	Utility::Panic("ERROR! Base Class: CharDevice::SgTTy()!"); // ����
}

/*==============================class ConsoleDevice===============================*/
/*
 * ���ﶨ��������ConsoleDevice�Ķ���ʵ����
 * ��ʵ��������override���ַ��豸������
 * Open(), Close(), Read(), Write()���麯����
 */
ConsoleDevice g_ConsoleDevice;
extern TTy g_TTy;

ConsoleDevice::ConsoleDevice() // ���캯��
{
	// nothing to do here
}

ConsoleDevice::~ConsoleDevice() // ��������
{
	// nothing to do here
}

// ���豸dev
void ConsoleDevice::Open(short dev, int mode)
{
	short minor = Utility::GetMinor(dev);	// ��ô��豸��
	User &u = Kernel::Instance().GetUser(); // ��ȡ��ǰ��user�ṹ

	if (minor != 0) /* ѡ��Ĳ���console */
	{
		return;
	}

	if (NULL == this->m_TTy) // ���Ŀǰָ���ַ��豸tty�ṹ��ָ����δ�գ���ָ�����̨ʵ��g_TTy
	{
		this->m_TTy = &g_TTy;
	}

	/* �ý��̵�һ�δ�����豸 */
	if (NULL == u.u_procp->p_ttyp)
	{
		u.u_procp->p_ttyp = this->m_TTy; // ��process�ṹ�е�p_ttyp��ֵ����ַΪ��ǰ��tty
	}

	/* �����豸��ʼģʽ */
	if ((this->m_TTy->t_state & TTy::ISOPEN) == 0)
	{
		this->m_TTy->t_state = TTy::ISOPEN | TTy::CARR_ON; // �����豸״̬��
		this->m_TTy->t_flags = TTy::ECHO;				   // �����ַ��豸������־��
		this->m_TTy->t_erase = TTy::CERASE;				   // ���ò������ַ�
		this->m_TTy->t_kill = TTy::CKILL;				   // ɾ�������ַ�
	}
}

// �ر��豸dev��ʲô��û��
void ConsoleDevice::Close(short dev, int mode)
{
	// nothing to do here
}

// ����̨������
void ConsoleDevice::Read(short dev)
{
	short minor = Utility::GetMinor(dev); // ��ô��豸��

	if (0 == minor)
	{
		// ���ѡ����console�������tty�ṹ�еĶ�����
		this->m_TTy->TTRead(); /* �ж��Ƿ�ѡ����console */
	}
}

// ����̨д����
void ConsoleDevice::Write(short dev)
{
	short minor = Utility::GetMinor(dev); // ��ô��豸��

	if (0 == minor)
	{
		// ���ѡ����console�������tty�ṹ�е�д����
		this->m_TTy->TTWrite(); /* �ж��Ƿ�ѡ����console */
	}
}

// ʲô������
void ConsoleDevice::SgTTy(short dev, TTy *pTTy)
{
}
