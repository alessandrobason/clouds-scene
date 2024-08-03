#include "tracelog.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "format.h"

#if COLLA_WIN
    #if COLLA_MSVC
        #pragma warning(disable:4996) // _CRT_SECURE_NO_WARNINGS.
    #endif

    #include <windows.h>
    
#if COLLA_CMT_LIB
    #pragma comment(lib, "User32")
#endif

    // avoid including windows.h
    
    #ifndef STD_OUTPUT_HANDLE
        #define STD_OUTPUT_HANDLE   ((DWORD)-11)
    #endif
    #ifndef CP_UTF8
        #define CP_UTF8             65001
    #endif

    #ifndef TLOG_NO_COLOURS
        #define TLOG_NO_COLOURS
    #endif
#endif

#if COLLA_EMC
    #define TLOG_NO_COLOURS
#endif

#if COLLA_EMC
    #define COLOUR_BLACK   "<span class=\"black\">"
    #define COLOUR_RED     "<span class=\"red\">"
    #define COLOUR_GREEN   "<span class=\"green\">"
    #define COLOUR_YELLOW  "<span class=\"yellow\">"
    #define COLOUR_BLUE    "<span class=\"blue\">"
    #define COLOUR_MAGENTA "<span class=\"magenta\">"
    #define COLOUR_CYAN    "<span class=\"cyan\">"
    #define COLOUR_WHITE   "<span class=\"white\">"
    #define COLOUR_RESET   "</span></b>"
    #define COLOUR_BOLD    "<b>"
#elif defined(TLOG_NO_COLOURS)
    #define COLOUR_BLACK   ""
    #define COLOUR_RED     ""
    #define COLOUR_GREEN   ""
    #define COLOUR_YELLOW  ""
    #define COLOUR_BLUE    ""
    #define COLOUR_MAGENTA ""
    #define COLOUR_CYAN    ""
    #define COLOUR_WHITE   ""
    #define COLOUR_RESET   ""
    #define COLOUR_BOLD    ""
#else
    #define COLOUR_BLACK   "\033[30m"
    #define COLOUR_RED     "\033[31m"
    #define COLOUR_GREEN   "\033[32m"
    #define COLOUR_YELLOW  "\033[33m"
    #define COLOUR_BLUE    "\033[22;34m"
    #define COLOUR_MAGENTA "\033[35m"
    #define COLOUR_CYAN    "\033[36m"
    #define COLOUR_WHITE   "\033[37m"
    #define COLOUR_RESET   "\033[0m"
    #define COLOUR_BOLD    "\033[1m"
#endif

#define MAX_TRACELOG_MSG_LENGTH 1024

bool use_newline = true;

#if COLLA_WIN
static void setLevelColour(int level) {
    WORD attribute = 15;
    switch (level) {
        case LogDebug:   attribute = 1; break; 
        case LogInfo:    attribute = 2; break;
        case LogWarning: attribute = 6; break;
        case LogError:   attribute = 4; break;
        case LogFatal:   attribute = 4; break;
    }

    HANDLE hc = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hc, attribute);
}
#else
static void setLevelColour(int level) {
    switch (level) {
        case LogDebug:   printf(COLOUR_BLUE); break; 
        case LogInfo:    printf(COLOUR_GREEN); break;
        case LogWarning: printf(COLOUR_YELLOW); break;
        case LogError:   printf(COLOUR_RED); break;
        case LogFatal:   printf(COLOUR_MAGENTA); break;
        default:         printf(COLOUR_RESET); break;
    }
}
#endif

void traceLog(int level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    traceLogVaList(level, fmt, args);
    va_end(args);
}

#ifdef TLOG_MUTEX
#include "cthreads.h"
static cmutex_t g_mtx = 0;
#endif

void traceLogVaList(int level, const char *fmt, va_list args) {
    #ifdef TLOG_MUTEX
    if (!g_mtx) g_mtx = mtxInit();
    mtxLock(g_mtx);
    #endif

    const char *beg = "";
    switch (level) {
        case LogTrace:   beg = "[TRACE"   ; break; 
        case LogDebug:   beg = "[DEBUG"   ; break; 
        case LogInfo:    beg = "[INFO"    ; break; 
        case LogWarning: beg = "[WARNING" ; break; 
        case LogError:   beg = "[ERROR"   ; break; 
        case LogFatal:   beg = "[FATAL"   ; break;        
        default:         break;
    }

#if COLLA_WIN
    SetConsoleOutputCP(CP_UTF8);
#endif
    setLevelColour(level);
    printf("%s", beg);

#if GAME_CLIENT
    putchar(':');
    traceSetColour(COL_CYAN);
    printf("CLIENT");
#elif GAME_HOST
    putchar(':');
    traceSetColour(COL_MAGENTA);
    printf("HOST");
#endif

    setLevelColour(level);
    printf("]: ");

    // set back to white
    setLevelColour(LogTrace);
    // vprintf(fmt, args);
    fmtPrintv(fmt, args);

    if(use_newline) {
#if COLLA_EMC
        puts("<br>");
#else
        puts("");
#endif
    }

#ifndef TLOG_DONT_EXIT_ON_FATAL
    if (level == LogFatal) {

#ifndef TLOG_NO_MSGBOX

#if COLLA_WIN
        char errbuff[1024];
        fmtBufferv(errbuff, sizeof(errbuff), fmt, args);

        char captionbuf[] = 
#if GAME_HOST
            "Fatal Host Error";
#elif GAME_CLIENT
            "Fatal Client Error";
#else
            "Fatal Error";
#endif
        MessageBoxA(
            NULL,
            errbuff, captionbuf,
            MB_ICONERROR | MB_OK
        );
#endif

#endif
        fflush(stdout);
        abort();
    }
#endif

#ifdef TLOG_MUTEX
    mtxUnlock(g_mtx);
#endif
}

void traceUseNewline(bool newline) {
    use_newline = newline;
}

void traceSetColour(colour_e colour) {
#if COLLA_WIN
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), colour);
#else
    switch (colour) {
        case COL_RESET:   printf(COLOUR_RESET);   break;
        case COL_BLACK:   printf(COLOUR_BLACK);   break;
        case COL_BLUE:    printf(COLOUR_BLUE);    break;
        case COL_GREEN:   printf(COLOUR_GREEN);   break;
        case COL_CYAN:    printf(COLOUR_CYAN);    break;
        case COL_RED:     printf(COLOUR_RED);     break;
        case COL_MAGENTA: printf(COLOUR_MAGENTA); break;
        case COL_YELLOW:  printf(COLOUR_YELLOW);  break;
    }
#endif
}