
#include <Windows.h>
#include <iostream>

#include "elf.h"
#include "StmApi.h"

int main()
{
    const char* elfFile = "C:/test.elf";

    uint32_t addr = 0;
    if (elfParse(elfFile))
    {
        addr = elfFindSymbol("startup");
    }

    if (addr > 0)
        printf("Found startup at 0x%x\r\n", addr);
    else
        printf("Failed to find startup in elf file %s\r\n", elfFile);


}

