// memory.cpp - Memory utilities for game modification

#include "memory.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "MCGG_Memory"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

uintptr_t Memory::GetModuleBase(const char* moduleName) {
    char line[256];
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, moduleName)) {
            uintptr_t base;
            if (sscanf(line, "%lx", &base) == 1) {
                fclose(fp);
                return base;
            }
        }
    }
    
    fclose(fp);
    return 0;
}

size_t Memory::GetModuleSize(const char* moduleName) {
    char line[256];
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;
    
    size_t totalSize = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, moduleName)) {
            uintptr_t start, end;
            if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                totalSize += (end - start);
            }
        }
    }
    
    fclose(fp);
    return totalSize;
}

bool Memory::SetProtection(uintptr_t address, size_t size, int prot) {
    uintptr_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t alignedAddr = address & ~(pageSize - 1);
    size_t alignedSize = ((address + size + pageSize - 1) & ~(pageSize - 1)) - alignedAddr;
    
    return mprotect(reinterpret_cast<void*>(alignedAddr), alignedSize, prot) == 0;
}

bool Memory::WriteMemory(uintptr_t address, void* data, size_t size) {
    if (!SetProtection(address, size, PROT_READ | PROT_WRITE | PROT_EXEC)) {
        return false;
    }
    
    memcpy(reinterpret_cast<void*>(address), data, size);
    return true;
}

bool Memory::NOP(uintptr_t address, size_t size) {
    // ARM64 NOP is 0xD503201F
    uint32_t nop = 0xD503201F;
    return WriteMemory(address, &nop, sizeof(nop));
}

bool Memory::Hook(uintptr_t target, void* detour, void** original) {
    // Simple inline hook for ARM64
    // This is a placeholder - use a proper hooking library like shadowhook/bytehook
    
    // For now, just return false as we need external hooking library
    return false;
}
