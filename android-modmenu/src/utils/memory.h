// memory.h
#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <cstddef>

namespace Memory {

uintptr_t GetModuleBase(const char* moduleName);
size_t GetModuleSize(const char* moduleName);
bool SetProtection(uintptr_t address, size_t size, int prot);
bool WriteMemory(uintptr_t address, void* data, size_t size);
bool NOP(uintptr_t address, size_t size);
bool Hook(uintptr_t target, void* detour, void** original);

} // namespace Memory

#endif // MEMORY_H
