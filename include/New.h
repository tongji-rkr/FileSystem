/*
 * @Description:
 * @Date: 2022-09-05 14:30:16
 * @LastEditTime: 2023-04-02 15:56:26
 */
#ifndef NEW_H
#define NEW_H

#include "KernelAllocator.h"

void set_kernel_allocator(KernelAllocator* pAllocator); // 设置内核分配器, 用于重载new和delete运算符
void* operator new (unsigned int size); // 重载new运算符, 用于分配内核内存
void operator delete (void* p); // 重载delete运算符, 用于释放内核内存

#endif

