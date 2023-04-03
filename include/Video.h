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

	static void TraceOn();	/*�����������*/
	static void TraceOff(); /*�رյ������*/

	static void Write(const char *fmt, ...); /* ��ʽ����� */
	static void ClearScreen();				 /* ���� */

private:
	static void PrintInt(unsigned int value, int base); /* ��ӡ���� */
	static void NextLine();								/* ���� */
	static void WriteChar(const char ch);				/* ����ַ� */

public:
	static unsigned int m_Row;	  /* ��ǰ��������� */
	static unsigned int m_Column; /* ��ǰ��������� */

private:
	static unsigned short *m_VideoMemory;
	/* Debug������� */
	static bool trace_on;
};

#endif
