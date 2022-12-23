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

//-----------------------------------------------------------------------------
// This is the wrapper for the STLink API
// 
// There is an annoying amount of junk in here that isn't used for anything
// except keeping the STM library code happy.
//
//-----------------------------------------------------------------------------


#include <stdarg.h>
#include <string>
#include <iostream>
#include "StmApi.h"

#define WIDH 50 
using namespace std;

#ifdef _WIN32
#include <Windows.h>
HANDLE  console = GetStdHandle(STD_OUTPUT_HANDLE);
CONSOLE_SCREEN_BUFFER_INFO SBInfo, CurSBInfo;
#endif

unsigned int Progress = 0;

// Number of times we allow a read to fail before giving up
#define FAIL_COUNT_MAX 3


#ifdef __cplusplus
extern "C" {
#endif

#if (defined WIN32 || defined _WIN32 || defined WINCE)
# define CP_EXPORTS __declspec(dllexport)
#else
# define CP_EXPORTS
#endif

typedef enum verbosity
{
    VERBOSITY_LEVEL_0 = 0,
    VERBOSITY_LEVEL_1 = 1,
    VERBOSITY_LEVEL_2 = 2,
    VERBOSITY_LEVEL_3 = 3
}verbosity;

// Floowing from CubeProgrammerAPI.h
typedef struct displayCallBacks
{
    void(*initProgressBar)();                              /**< Add a progress bar. */
    void(*logMessage)(int msgType, const wchar_t* str);   /**< Display internal messages according to verbosity level. */
    void(*loadBar)(int x, int n);                          /**< Display the loading of read/write process. */
} displayCallBacks;

void setLoadersPath(const char* path);

typedef enum debugPort
{
    JTAG = 0,               /**< JTAG debug port. */
    SWD = 1,                /**< SWD debug port. */
}debugPort;

typedef enum debugConnectMode
{
    NORMAL_MODE,            /**< Connect with normal mode, the target is reset then halted while the type of reset is selected using the [debugResetMode]. */
    HOTPLUG_MODE,           /**< Connect with hotplug mode,  this option allows the user to connect to the target without halt or reset. */
    UNDER_RESET_MODE,       /**< Connect with under reset mode, option allows the user to connect to the target using a reset vector catch before executing any instruction. */
    POWER_DOWN_MODE,        /**< Connect with power down mode. */
    PRE_RESET_MODE          /**< Connect with pre reset mode. */
}debugConnectMode;

typedef enum debugResetMode
{
    SOFTWARE_RESET,         /**< Apply a reset by the software. */
    HARDWARE_RESET,         /**< Apply a reset by the hardware. */
    CORE_RESET              /**< Apply a reset by the internal core peripheral. */
}debugResetMode;

typedef struct frequencies
{
    unsigned int jtagFreq[12];          /**<  JTAG frequency. */
    unsigned int jtagFreqNumber;        /**<  Get JTAG supported frequencies. */
    unsigned int swdFreq[12];           /**<  SWD frequency. */
    unsigned int swdFreqNumber;         /**<  Get SWD supported frequencies. */
}frequencies;

typedef struct debugConnectParameters
{
    debugPort dbgPort;                  /**< Select the type of debug interface #debugPort. */
    int index;                          /**< Select one of the debug ports connected. */
    char serialNumber[33];              /**< ST-LINK serial number. */
    char firmwareVersion[20];           /**< Firmware version. */
    char targetVoltage[5];              /**< Operate voltage. */
    int accessPortNumber;               /**< Number of available access port. */
    int accessPort;                     /**< Select access port controller. */
    debugConnectMode connectionMode;    /**< Select the debug CONNECT mode #debugConnectMode. */
    debugResetMode resetMode;           /**< Select the debug RESET mode #debugResetMode. */
    int isOldFirmware;                  /**< Check Old ST-LINK firmware version. */
    frequencies freq;                   /**< Supported frequencies #frequencies. */
    int frequency;                      /**< Select specific frequency. */
    int isBridge;                       /**< Indicates if it's Bridge device or not. */
    int shared;                         /**< Select connection type, if it's shared, use ST-LINK Server. */
    char board[100];                    /**< board Name */
    int DBG_Sleep;
}debugConnectParameters;

void setDisplayCallbacks(displayCallBacks c);
void setVerbosityLevel(int level);
int getStLinkList(debugConnectParameters** stLinkList, int shared);
int connectStLink(debugConnectParameters debugParameters);
int reset(debugResetMode rstMode);
int getStLinkList(debugConnectParameters** stLinkList, int shared);
void disconnect();
int readMemory(unsigned int address, unsigned char** data, unsigned int size);
void deleteInterfaceList();

#ifdef __cplusplus
}
#endif


#define STM_API_DEBUG

//-----------------------------------------------------------------------------
// Used for debugging the STM API
//-----------------------------------------------------------------------------
void InitPBar()
{
#ifdef STM_API_DEBUG

    for (int idx = 0; idx < WIDH; idx++)
    {
        putc(177, stdout);
    }
    printf(" %d%c", Progress, '%');
    putc('\r', stdout);
    Progress = 0;

#endif // STM_API_DEBUG
}

void lBar(int currProgress, int total)
{
#ifdef STM_API_DEBUG
    unsigned int alreadyLoaded = 0;
    if (total == 0) return;
    if (currProgress > total)
        currProgress = total;

    /*Calculuate the ratio of complete-to-incomplete.*/
    float ratio = currProgress / (float)total;
    unsigned int   counter = (unsigned int)ratio * WIDH;

    if (counter > alreadyLoaded)
    {
        for (DWORD Idx = Progress; Idx < (counter - alreadyLoaded); Idx++)
        {
            putc(219, stdout);
        }
        Progress = counter;

        printf(" %d%%", (int)(ratio * 100));
    }
    alreadyLoaded = counter;
#endif //STM_API_DEBUG
}


void DisplayMessage(int msgType, const wchar_t* str)
{
#ifdef STM_API_DEBUG
        wprintf(L"%s\n", str);
#endif // STM_API_DEBUG
}

void logMessage(int msgType, const char* str, ...)
{
#ifdef STM_API_DEBUG
    va_list args;
    va_start(args, str);
    char buffer[256];
    vsprintf_s(buffer, sizeof(buffer), str, args);
    printf("%s", buffer);
    va_end(args);
#endif // STM_API_DEBUG
}

//-----------------------------------------------------------------------------
// Open the STLink
//
// Opens in "shared" mode so you must be running STLink Server.
//        See: https://www.st.com/en/development-tools/st-link-server.html
//
// Assumes only one viable STLink on the PC
//
// Return True is successful
//-----------------------------------------------------------------------------
bool stmOpen(const char* stmFlashLoader)
{
    // Even if you have no intention of writing to the flash, you must provide
    // a path to the FlashLoader directory.
    // You can commonly find this in the installed IDE at:
    //     C:\ST\STM32CubeIDE....\tools\bin\FlashLoader
    displayCallBacks vsLogMsg;
    setLoadersPath(stmFlashLoader);
    vsLogMsg.logMessage = DisplayMessage;
    vsLogMsg.initProgressBar = InitPBar;
    vsLogMsg.loadBar = lBar;
    setDisplayCallbacks(vsLogMsg);
    setVerbosityLevel(VERBOSITY_LEVEL_1);

    debugConnectParameters *stLinkList;
    debugConnectParameters debugParameters;
    //generalInf* genInfo;

    int getStlinkListNb = getStLinkList(&stLinkList, 1);

    if (getStlinkListNb == 0)
    {
        // No STLink found
        return false;
    }

    // Connect in Hotplug mode so the device can be already running
    // and shared mode so you can use the IDE at teh same time.
    debugParameters = stLinkList[0];
    debugParameters.connectionMode = HOTPLUG_MODE;
    debugParameters.shared = 1;

    int connectStlinkFlag = connectStLink(debugParameters);
    if (connectStlinkFlag != 0)
    {
        // Could not connect to the STLink we found
        disconnect();
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// Get data from the remote system 
//
// Passes in a Ptr-to-Ptr so tha we can return the pointer to the internal
// buffer hlding the data.
//
// The STM DLL is a little od in that it retains ownership of this buffer,
// presumeably re-using it next time.
//
// Returns the number of 32 bit words read.
//
// It will read all the words it finds in the buffer to minimise latency,
// but only whole words.
//-----------------------------------------------------------------------------
uint32_t stmGetData(uint32_t addr, uint32_t length, uint32_t *rxBuffer)
{
    unsigned char* dataStruct = 0;
    int readMemoryFlag = readMemory(addr, &dataStruct, length);
    memcpy(rxBuffer, dataStruct, length);

    if (readMemoryFlag == 0)
    {
        return length;
    }
    else
    {
        return 0;
    }
}

//-----------------------------------------------------------------------------
// Close and recover any memory allocated
//
// This is seldom called because Wireshark summarily terminates the App at the
// moment.
//-----------------------------------------------------------------------------
void stmClose(void)
{
    disconnect();
    deleteInterfaceList();
}
