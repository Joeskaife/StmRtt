/*
    This is free and unencumbered software released into the public domain.

    Anyone is free to copy, modify, publish, use, compile, sell, or
    distribute this software, either in source code form or as a compiled
    binary, for any purpose, commercial or non-commercial, and by any
    means.

    In jurisdictions that recognize copyright laws, the author or authors
    of this software dedicate any and all copyright interest in the
    software to the public domain. We make this dedication for the benefit
    of the public at large and to the detriment of our heirs and
    successors. We intend this dedication to be an overt act of
    relinquishment in perpetuity of all present and future rights to this
    software under copyright law.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
    OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.

    For more information, please refer to <https://unlicense.org>
*/

#include <iostream>
#include <fstream>
#include <stdint.h>
#include "elf.h"


typedef struct
{
    const char* symbolName;
    uint32_t symbolAddr;
}symbolTable_t;

symbolTable_t *symbolTable;
int symbolTableLen = 0;

// A bit arbitrary but so far has only been 3
Elf32_Phdr elfPrgHdr[100];
uint8_t *ldRegions[100];
char* stringTable = NULL;
int numLdRegions = 0;

int numSymbols = 0;

// ----------------------------------------------------------------------------
// Get a pointer that corresponds to the given address in the target memory.
//
// This is a pointer into an allocated array, populated by parseElf
// ----------------------------------------------------------------------------
uint8_t *elfGetLdData(uint32_t addr)
{
    uint8_t *dataPtr = NULL;
    for (int i = 0; i < numLdRegions; i++)
    {
        if ((elfPrgHdr[i].p_vaddr <= addr) &&
            (elfPrgHdr[i].p_vaddr + elfPrgHdr[i].p_memsz) >= addr)
        {
            dataPtr = ldRegions[i] + (addr - elfPrgHdr[i].p_vaddr);
            break;
        }
    }
    return dataPtr;
}

// ----------------------------------------------------------------------------
// Clean up allocations made by parseElf()
// ----------------------------------------------------------------------------
void elfFree(void)
{
    for (int i = 0; i < numLdRegions; i++)
    {
        if (ldRegions[i] != NULL)
        {
            free(ldRegions[i]);
            ldRegions[i] = NULL;
        }
    }
    free(stringTable);
    free(symbolTable);
}

// ----------------------------------------------------------------------------
// Parse the given elf file
// Populate a collection of allocated buffers with the program data from the
// elf.
// Return the address in the program data area corresponding to the given text
// symbol name.
//
// Returns 0 on Failure
// ----------------------------------------------------------------------------
bool elfParse(const char* elfFile)
{
    std::ifstream inputFile(elfFile, std::ios::binary);

    if (!inputFile.good())
    {
        printf("Could not open %s\r\n", elfFile);
        return false;
    }
    // get its size:
    inputFile.seekg(0, std::ios::end);
    std::streampos fileSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);

    // read the header:
    Elf32_Ehdr elfHdr;
    inputFile.read((char*)&elfHdr, sizeof Elf32_Ehdr);

    uint32_t entryPoint;

    // Have we got a valid elf file?
    if ((elfHdr.e_ident[EI_MAG0] == ELFMAG0) &&
        (elfHdr.e_ident[EI_MAG1] == ELFMAG1) &&
        (elfHdr.e_ident[EI_MAG2] == ELFMAG2) &&
        (elfHdr.e_ident[EI_MAG3] == ELFMAG3))
    {

        // 64bit elf not currently supported
        bool is64Elf = false;
        if (elfHdr.e_ident[EI_CLASS] == ELFCLASS64)
        {
            // We've got ourselves an elf-64!
            is64Elf = true;
            inputFile.seekg(0, std::ios::beg);
            Elf64_Ehdr elfHdr64;
            inputFile.read((char*)&elfHdr64, sizeof Elf64_Ehdr);

            // #todo
            return false;
        }
        else
        {
            entryPoint = elfHdr.e_entry;
        }
    }

    // Read the program regions into allocated memory
    numLdRegions = elfHdr.e_phnum;
    for (int i = 0; i < elfHdr.e_phnum; i++)
    {
        inputFile.read((char*)&elfPrgHdr[i], sizeof Elf32_Phdr);

        if (elfPrgHdr[i].p_filesz)
        {
            ldRegions[i] = (uint8_t*)malloc(elfPrgHdr[i].p_filesz);
            inputFile.seekg(elfPrgHdr[i].p_offset, std::ios::beg);
            inputFile.read((char*)ldRegions[i], elfPrgHdr[i].p_filesz);
        }
    }

    // Arbitrary guess to maximum number of sections.
    Elf32_Shdr elfSectionHdr[100];

    // Read the string tables so we can look symbols up by name
    int stringTableSize = 0;
    int symbolTableSize = 0;
    inputFile.seekg(elfHdr.e_shoff, std::ios::beg);

    // Go through the section table to add up the size of the symbol table
    for (int i = 0; i < elfHdr.e_shnum; i++)
    {
        inputFile.read((char*)&elfSectionHdr[i], sizeof Elf32_Shdr);

        if (elfSectionHdr[i].sh_type == SHT_STRTAB)
        {
            stringTableSize += elfSectionHdr[i].sh_size;
        }

        if (elfSectionHdr[i].sh_type == SHT_SYMTAB)
        {
            symbolTableSize += elfSectionHdr[i].sh_size;
        }
    }

    // Now we know how big it is, allocate space for it
    stringTable = (char*)malloc(stringTableSize);
    char* stringTablePtr = stringTable;

    symbolTableLen = symbolTableSize / sizeof Elf32_Sym;
    symbolTable = (symbolTable_t*)malloc(sizeof(symbolTable_t) * symbolTableLen);
    symbolTable_t *symbolTable_p = symbolTable;

    // Now we have space for the symbol table, go through and populate it
    for (int i = 0; i < elfHdr.e_shnum; i++)
    {
        if (elfSectionHdr[i].sh_type == SHT_STRTAB)
        {
            inputFile.seekg(elfSectionHdr[i].sh_offset, std::ios::beg);
            inputFile.read(stringTablePtr, elfSectionHdr[i].sh_size);
            stringTablePtr += elfSectionHdr[i].sh_size;
            //printf("Section name: %s of 0x%x, %d at 0x%x\n", (char*)(stringTable + elfSectionHdr[i].sh_name), elfSectionHdr[i].sh_size, elfSectionHdr->sh_entsize, elfSectionHdr[i].sh_addr);
        }
    }

    // Now for the complicated bit. Populate an array of structures mapping symbol pointers
    // to addresses in the remote system. Unfortunately the name string isn't stored in the symbol
    // table, just an index to it in the string table, which is why we loaded that first.
    for (int i = 0; i < elfHdr.e_shnum; i++)
    {
        if (elfSectionHdr[i].sh_type == SHT_SYMTAB)
        {
            //printf("Section: %s of %d 0x%x, %d at 0x%x\n", (char*)(stringTable + elfSectionHdr[i].sh_name), elfSectionHdr[i].sh_type, elfSectionHdr[i].sh_size, elfSectionHdr->sh_entsize, elfSectionHdr[i].sh_addr);
            inputFile.seekg(elfSectionHdr[i].sh_offset, std::ios::beg);
            for (uint32_t symbolIndex = 0; symbolIndex < elfSectionHdr[i].sh_size / sizeof Elf32_Sym; symbolIndex++)
            {
                Elf32_Sym symbol;
                inputFile.read((char*)&symbol, sizeof Elf32_Sym);
                stringTablePtr += elfSectionHdr[i].sh_size;
                printf("Symbol: %s, 0x%x\n", (char*)(stringTable + symbol.st_name), (uint32_t)symbol.st_value);

                symbolTable_p->symbolAddr = (uint32_t)symbol.st_value;
                symbolTable_p->symbolName = (char*)(stringTable + symbol.st_name);
                symbolTable_p++;
            }
        }
    }

    return true;
}

uint32_t elfFindSymbol(const char* symbolName)
{
    for (int i = 0; i < symbolTableLen; i++)
    {
        if (strstr(symbolTable[i].symbolName, symbolName))
        {
            return symbolTable[i].symbolAddr;
        }
    }
    return 0;
}