#pragma once

#define XTERM_RAW(COLOR) "\e[38;5;" #COLOR "m"
#define XTERM_BLOCK(COLOR_STR, STR) COLOR_STR STR XTERM_RAW(7)

#define XTERM_BLACK        XTERM_RAW(0)
#define XTERM_DARK_RED     XTERM_RAW(1)
#define XTERM_DARK_GREEN   XTERM_RAW(2)
#define XTERM_DARK_YELLOW  XTERM_RAW(3)
#define XTERM_DARK_BLUE    XTERM_RAW(4)
#define XTERM_PURPLE       XTERM_RAW(5)
#define XTERM_TRUQUOISE    XTERM_RAW(6)
#define XTERM_WHITE        XTERM_RAW(7)
#define XTERM_GRAY         XTERM_RAW(8)
#define XTERM_RED          XTERM_RAW(9)
#define XTERM_GREEN        XTERM_RAW(10)
#define XTERM_YELLOW       XTERM_RAW(11)
#define XTERM_BLUE         XTERM_RAW(12)
#define XTERM_PINK         XTERM_RAW(13)
#define XTERM_CYAN         XTERM_RAW(14)
#define XTERM_BRIGHT_WHITE XTERM_RAW(15)


#define log_setCurrentHeaderThreshold(thres) \
    log_setHeaderThreshold(__FILE__, thres)
#define log_levelIsVisibleCurrentHeader(level) \
    log_levelIsVisibleHeader(__FILE__, level)
#define logMsg(level, fmt, ...) \
    { \
        if(log_levelIsVisibleCurrentHeader(level)) \
            _log(level, fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    }

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "./dsa.h"


typedef enum LogLevelEnum {
    LOG_LVL_DEBUG = 0,
    LOG_LVL_INFO  = 1,
    LOG_LVL_WARN  = 2,
    LOG_LVL_ERR   = 3,
    LOG_LVL_FATAL = 4
} LogLevel;

typedef enum LogStyleEnum {
    LOG_STYLE_COLOR,
    LOG_STYLE_NO_COLOR
} LogStyle;


void log_setLogStyle(LogStyle style);
void log_setGlobalThreshold(LogLevel thres);
void log_setHeaderThreshold(const char *strHeaderFile, LogLevel thres);
uint8_t log_levelIsVisibleHeader(const char *strHeaderFile, LogLevel level);

void _log(LogLevel level, const char *format, const char *strHeaderFile,
          const char *strFun, int line, ...);