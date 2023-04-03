/*
 * @Description:
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */
// Video.h
#ifndef DIAGNOSE_H
#define DIAGNOSE_H

class Diagnose
{
public:
	static unsigned int ROWS;

	/* static const member */
	static const unsigned int COLUMNS = 80;		/* full screen columns */
	static const unsigned short COLOR = 0x0B00; /* char in bright CYAN */
	static const unsigned int SCREEN_ROWS = 25; /* full screen rows */

public:
	Diagnose();	 /* Constructor */
	~Diagnose(); /* Destructor */

	static void TraceOn();	/*启动调试输出*/
	static void TraceOff(); /*关闭调试输出*/

	static void Write(const char *fmt, ...); /* 格式化输出 */
	static void ClearScreen();				 /* 清屏 */

private:
	static void PrintInt(unsigned int value, int base); /* 打印整数 */
	static void NextLine();								/* 换行 */
	static void WriteChar(const char ch);				/* 输出字符 */

public:
	static unsigned int m_Row;	  /* 当前光标所在行 */
	static unsigned int m_Column; /* 当前光标所在列 */

private:
	static unsigned short *m_VideoMemory;
	/* Debug输出开关 */
	static bool trace_on;
};

#endif
