#ifndef QMI_DIALER_H
#define QMI_DIALER_H

#include <stdint.h>
#include <stdio.h>
#include <time.h> 

#define QMID_LOG_PREFIX "[%d:%d:%d %d/%d/%d %s:%d]: "
#ifdef __FILENAME__
    #define QMID_LOG_PREFIX_ARG __FILENAME__, __LINE__
#else
    #define QMID_LOG_PREFIX_ARG __FILE__, __LINE__
#endif
#define QMID_DEBUG_PRINT2(fd, ...){fprintf(fd, __VA_ARGS__);fflush(fd);}
//The ## is there so that I dont have to fake an argument when I use the macro
//on string without arguments! It removes the comma and, thus, the macro expands
//just fine. See: http://gcc.gnu.org/onlinedocs/gcc/Variadic-Macros.html
//
//The reason the semi-colon at the end of the macro is omitted, is for
//consistency. The macro should appear and be used as a normal function.
//Imagine the following:
//if(<cond)
//  QMID_DEBUG_PRINT("");
//else
//  printf("Hei");
//
//If I add the semicolon to the macro, I would have to remove the semi-colon in
//the code. I.e., macros would have to get special treatment when programming.
//Two semicolons will cause the else to be dangling and a compilation error,
//unless brackets are added
#define QMID_DEBUG_PRINT(fd, _fmt, ...) \
    do { \
        time_t rawtime; \
        struct tm *curtime; \
        time(&rawtime); \
        curtime = gmtime(&rawtime); \
        \
        QMID_DEBUG_PRINT2(fd, QMID_LOG_PREFIX _fmt, curtime->tm_hour, \
        curtime->tm_min, curtime->tm_sec, curtime->tm_mday, \
        curtime->tm_mon + 1, 1900 + curtime->tm_year, \
        QMID_LOG_PREFIX_ARG, ##__VA_ARGS__); \
    } while(0)

//Log levels
enum{
    QMID_LOG_LEVEL_NONE = 0, //Only output if application fails
    QMID_LOG_LEVEL_1, //Output essential information, like connected/disconnected and technology changes
    QMID_LOG_LEVEL_2, //Output everything the application does (for example msg)
    QMID_LOG_LEVEL_3, //Output all packages (type and content)
    QMID_LOG_LEVEL_MAX, //This is not merged with top level to easy adding new levels
};

//Global variable controlling log level (binary for now)
uint8_t qmid_verbose_logging;

#endif
