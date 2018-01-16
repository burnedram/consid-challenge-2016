#pragma comment(lib, "psapi.lib")

#include <stdio.h>
#include <Windows.h>

#include "log.h"

#ifdef _DEBUG
#define LOG(...) printf(__VA_ARGS__)
#else
#define NO_ERROR_CHECKS
#define LOG(...) do {} while(0)
#endif

#ifdef NO_ERROR_CHECKS
#define ERROR_CHECK(check, errormsg, exitcode, ...) do {} while(0)
#else
#define ERROR_CHECK(check, errormsg, exitcode, ...) \
    if (check) { \
    	printf(" " #check ": " errormsg "\n", __VA_ARGS__); \
    	return exitcode; \
    } do {} while(0)
#endif

#define REG_STR_LENGTH 6 // ABC123
#define REG_LINE_JUNK 2 // CRLF

typedef UINT32 reg_t;

#define NEW_REG_STR calloc(7, sizeof(char));
#define READ_REG_STR(pStr, pReg) memcpy(pStr, pReg, 6);
#define READ_REG(pData) (                                                                        \
                        ((*(pData + 0) - 'A') << (REG_CHAR_BITS * 0))                      |     \
    		   		    ((*(pData + 1) - 'A') << (REG_CHAR_BITS * 1))                      |     \
    		   		    ((*(pData + 2) - 'A') << (REG_CHAR_BITS * 2))                      |     \
    		   		    ((*(pData + 3) - '0') << (REG_CHAR_BITS * 3 + REG_DIGIT_BITS * 0)) |     \
    		   		    ((*(pData + 4) - '0') << (REG_CHAR_BITS * 3 + REG_DIGIT_BITS * 1)) |     \
    		   		    ((*(pData + 5) - '0') << (REG_CHAR_BITS * 3 + REG_DIGIT_BITS * 2))       \
                       )

// Disable C4293: shift count negative or too big, undefined behavior
#pragma warning( push )
#pragma warning( disable : 4293 )

// Number of bits to represent one character
#define REG_CHAR_BITS BITS_TO_REPRESENT('Z' - 'A' + 1)

// Number of bits to represent on digit
#define REG_DIGIT_BITS BITS_TO_REPRESENT('9' - '0' + 1)

// Number of bits to represent a complete number plate (ABC123)
#define REG_BITS (REG_CHAR_BITS + REG_CHAR_BITS + REG_CHAR_BITS + REG_DIGIT_BITS + REG_DIGIT_BITS + REG_DIGIT_BITS)

// Number plate counters
volatile BYTE regCounts[1 << REG_BITS];
#pragma warning( pop )

typedef struct THREAD_ARGS {
    char *pReg;
    char *pEndOfReg;
} THREAD_ARGS;

DWORD WINAPI threadfunc(void *args);

int main(int argc, char **argv) {
#ifdef _DEBUG
    char *pszFilename = "..\\Rgn00.txt";
#else
    if (argc < 2) {
    	printf("Specify an input file");
    	return 1;
    }
    char *pszFilename = argv[1];
#endif
    LOG("Opening file \"%s\"...", pszFilename);
    HANDLE hFile = CreateFileA(pszFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    ERROR_CHECK(hFile == INVALID_HANDLE_VALUE, "%d", 2, GetLastError());
    DWORD dwFileSize = GetFileSize(hFile, NULL);
    ERROR_CHECK(dwFileSize == INVALID_FILE_SIZE, "%d", 3, GetLastError());
    LOG(" OK\n");

    LOG("Mapping to memory...");
    HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    ERROR_CHECK(hMapping == NULL, "%d", 4, GetLastError());
    char *pView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    ERROR_CHECK(pView == NULL, "%d", 5, GetLastError());
    LOG(" OK\n");

#ifndef _DEBUG
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS); // All your resoures are belong to us
#endif

#ifndef _DEBUG
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    DWORD nThreads = sysinfo.dwNumberOfProcessors;
#else
	DWORD nThreads = 1;
#endif
    DWORD sliceSize = dwFileSize / nThreads;
    LOG("Creating %d threads...", nThreads);
    THREAD_ARGS *pThreadArgs = malloc(sizeof(THREAD_ARGS) * nThreads);
    HANDLE *hThreads = malloc(sizeof(HANDLE) * nThreads);
    for (DWORD i = 0; i < nThreads; i++) {
        pThreadArgs[i].pReg = pView + sliceSize * i;
        pThreadArgs[i].pEndOfReg = i == nThreads - 1 ? pView + dwFileSize : pThreadArgs[i].pReg + sliceSize;
        hThreads[i] = CreateThread(NULL, 0, threadfunc, &pThreadArgs[i], 0, NULL);
        ERROR_CHECK(hThreads[i] == NULL, "i = %d, %d", 6, i, GetLastError());
    }
    LOG(" OK\n");

    WaitForMultipleObjects(nThreads, hThreads, TRUE, INFINITE);
    printf("Ej dubbletter\n");
    return 0;
}

DWORD WINAPI threadfunc(void *args) {
    THREAD_ARGS *targs = args;
    char *pReg = targs->pReg;
    char *pEndOfReg = targs->pEndOfReg;
#ifdef _DEBUG
    char *pszReg = NEW_REG_STR;
#endif
    for (; pEndOfReg - pReg > 0; pReg += REG_STR_LENGTH + REG_LINE_JUNK) {
		reg_t reg = READ_REG(pReg);
#ifdef _DEBUG
        READ_REG_STR(pszReg, pReg);
        LOG("%s: %d\n", pszReg, reg);
#endif
        BYTE count = InterlockedIncrement16((volatile SHORT *) &regCounts[reg]) & 0xFF;
        if (count > 1) {
			printf("Dubbletter\n");
			ExitProcess(0);
		}
    }
    return 0;
}