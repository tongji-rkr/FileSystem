#include "Kernel.h"
#include <iostream>
#include <string>
#include <string.h>
using namespace std;

int main()
{ 
    // 初始化内核
    Kernel::Instance().Initialize();
    return 0;
}


