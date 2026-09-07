// logger.cpp - Logging utilities
#include "logger.h"

void LogI(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, tag, fmt, args);
    va_end(args);
}

void LogE(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, tag, fmt, args);
    va_end(args);
}

void LogW(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_WARN, tag, fmt, args);
    va_end(args);
}
