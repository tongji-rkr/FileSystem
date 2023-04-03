/*
 * @Description:
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:58
 */
#ifndef CHAR_DEVICE_H
#define CHAR_DEVICE_H

#include "TTy.h"

// 字符设备类
class CharDevice
{
public:
	CharDevice();		   // 构造函数
	virtual ~CharDevice(); // 析构函数
	/*
	 * 定义为虚函数，由派生类进行override实现设备
	 * 特定操作。正常情况下，基类中函数不应被调用到。
	 */
	virtual void Open(short dev, int mode);	  // 基类中不被调用
	virtual void Close(short dev, int mode);  // 基类中不被调用
	virtual void Read(short dev);			  // 基类中不被调用
	virtual void Write(short dev);			  // 基类中不被调用
	virtual void SgTTy(short dev, TTy *pTTy); // 基类中不被调用

public:
	TTy *m_TTy; /* 指向字符设备TTy结构的指针 */
};

class ConsoleDevice : public CharDevice
{
public:
	ConsoleDevice();		  // 构造函数
	virtual ~ConsoleDevice(); // 析构函数
	/*
	 * Override基类CharDevice中的虚函数，实现
	 * 派生类ConsoleDevice特定的设备操作逻辑。
	 */
	void Open(short dev, int mode);	  // 打开设备
	void Close(short dev, int mode);  // 关闭设备
	void Read(short dev);			  // 如果设备是tty，执行tty设备通用读函数
	void Write(short dev);			  // 如果设备是tty，执行tty设备通用写函数
	void SgTTy(short dev, TTy *pTTy); // 为空
};

#endif
