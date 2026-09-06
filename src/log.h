#pragma once
// Logging ke logcat. Tag "MCGGMOD" -> filter: adb logcat -s MCGGMOD
#include <android/log.h>

#define MCGG_TAG "MCGGMOD"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  MCGG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  MCGG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, MCGG_TAG, __VA_ARGS__)
