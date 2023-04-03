/*
 * @Description: this file defines the class CharDevice
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */
#include "CharDevice.h"
#include "Utility.h"
#include "Kernel.h"

/*==============================class CharDevice===============================*/
CharDevice::CharDevice() // 构造函数
{
	this->m_TTy = NULL; // 指向字符设备TTY结构的指针置空
}

CharDevice::~CharDevice() // 析构函数
{
	// nothing to do here
}

void CharDevice::Open(short dev, int mode) // 不应被执行
{
	Utility::Panic("ERROR! Base Class: CharDevice::Open()!"); // 报错
}

void CharDevice::Close(short dev, int mode) // 不应被执行
{
	Utility::Panic("ERROR! Base Class: CharDevice::Close()!"); // 报错
}

void CharDevice::Read(short dev) // 不应被执行
{
	Utility::Panic("ERROR! Base Class: CharDevice::Read()!"); // 报错
}

void CharDevice::Write(short dev) // 不应被执行
{
	Utility::Panic("ERROR! Base Class: CharDevice::Write()!"); // 报错
}

void CharDevice::SgTTy(short dev, TTy *pTTy) // 不应被执行
{
	Utility::Panic("ERROR! Base Class: CharDevice::SgTTy()!"); // 报错
}

/*==============================class ConsoleDevice===============================*/
/*
 * 这里定义派生类ConsoleDevice的对象实例。
 * 该实例对象中override了字符设备基类中
 * Open(), Close(), Read(), Write()等虚函数。
 */
ConsoleDevice g_ConsoleDevice;
extern TTy g_TTy;

ConsoleDevice::ConsoleDevice() // 构造函数
{
	// nothing to do here
}

ConsoleDevice::~ConsoleDevice() // 析构函数
{
	// nothing to do here
}

// 打开设备dev
void ConsoleDevice::Open(short dev, int mode)
{
	short minor = Utility::GetMinor(dev);	// 获得次设备号
	User &u = Kernel::Instance().GetUser(); // 获取当前的user结构

	if (minor != 0) /* 选择的不是console */
	{
		return;
	}

	if (NULL == this->m_TTy) // 如果目前指向字符设备tty结构的指针仍未空，则指向控制台实例g_TTy
	{
		this->m_TTy = &g_TTy;
	}

	/* 该进程第一次打开这个设备 */
	if (NULL == u.u_procp->p_ttyp)
	{
		u.u_procp->p_ttyp = this->m_TTy; // 给process结构中的p_ttyp赋值，地址为当前的tty
	}

	/* 设置设备初始模式 */
	if ((this->m_TTy->t_state & TTy::ISOPEN) == 0)
	{
		this->m_TTy->t_state = TTy::ISOPEN | TTy::CARR_ON; // 设置设备状态字
		this->m_TTy->t_flags = TTy::ECHO;				   // 设置字符设备工作标志字
		this->m_TTy->t_erase = TTy::CERASE;				   // 设置擦除键字符
		this->m_TTy->t_kill = TTy::CKILL;				   // 删除整行字符
	}
}

// 关闭设备dev，什么都没做
void ConsoleDevice::Close(short dev, int mode)
{
	// nothing to do here
}

// 控制台读函数
void ConsoleDevice::Read(short dev)
{
	short minor = Utility::GetMinor(dev); // 获得次设备号

	if (0 == minor)
	{
		// 如果选择了console，则调用tty结构中的读函数
		this->m_TTy->TTRead(); /* 判断是否选择了console */
	}
}

// 控制台写函数
void ConsoleDevice::Write(short dev)
{
	short minor = Utility::GetMinor(dev); // 获得次设备号

	if (0 == minor)
	{
		// 如果选择了console，则调用tty结构中的写函数
		this->m_TTy->TTWrite(); /* 判断是否选择了console */
	}
}

// 什么都不做
void ConsoleDevice::SgTTy(short dev, TTy *pTTy)
{
}
