#include "logger.h"


static LogStyle g_logStyle = LOG_STYLE_COLOR;
static LogLevel g_logThres = LOG_LVL_DEBUG;
static Hashmap g_headerThresLevels = {0, 0, 0};


void log_setLogStyle(const LogStyle style) {
    g_logStyle = style;
}

void log_setGlobalThreshold(const LogLevel thres) {
    g_logThres = thres;
}

void log_setHeaderThreshold(const char *const strHeaderFile, const LogLevel thres) {
    HashmapVal hmapVal = (HashmapVal){(uint32_t)thres};
    hashmap_set(&g_headerThresLevels, str_hash(strHeaderFile), hmapVal);
}
 
uint8_t log_levelIsVisibleHeader(const char *const strHeaderFile, const LogLevel level) {
    LogLevel headerThres = LOG_LVL_DEBUG;
    hashmap_getU32(&g_headerThresLevels, str_hash(strHeaderFile), &headerThres);
    return level >= g_logThres && level >= headerThres;
}

void _log(const LogLevel level, const char *const format,
          const char *const strHeaderFile, const char *const strFun,
          int line, ...) {
    const static char *levelColor[] = {
        XTERM_GRAY, XTERM_BLUE, XTERM_YELLOW, XTERM_RED, XTERM_PURPLE
    };
    const static char *levelStr[] = {
        "[DEBUG]", "[INFO]", "[WARNING]", "[ERROR]", "[FATAL]"
    };

    va_list args;
    va_start(args, line);

    LogLevel headerThres = LOG_LVL_DEBUG;
    hashmap_getU32(&g_headerThresLevels, str_hash(strHeaderFile), &headerThres);

    if(log_levelIsVisibleHeader(strHeaderFile, level)) {
        if(g_logStyle == LOG_STYLE_COLOR) {
            printf(
                "%s%s " XTERM_GRAY "(%s:%s:%d) %s",
                levelColor[level], levelStr[level], strHeaderFile, strFun, line,
                levelColor[level]
            );
            vprintf(format, args);
            printf(XTERM_WHITE "\n");
        }
        else {
            printf(
                "%s (%s:%s:%d) ",
                levelStr[level], strHeaderFile, strFun, line
            );
            vprintf(format, args);
            printf("\n");
        }
    }

    va_end(args);
    if(level == LOG_LVL_FATAL)
        exit(1);
}