// logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <android/log.h>
#include <stdarg.h>

void LogI(const char* tag, const char* fmt, ...);
void LogE(const char* tag, const char* fmt, ...);
void LogW(const char* tag, const char* fmt, ...);

#endif // LOGGER_H
