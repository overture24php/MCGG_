// il2cpp_api.h
#ifndef IL2CPP_API_H
#define IL2CPP_API_H

#include <cstdint>

bool IL2CPP_Initialize();
void* IL2CPP_FindClass(const char* namespaceName, const char* className);
void* IL2CPP_FindMethod(void* klass, const char* methodName, int argc);
uintptr_t IL2CPP_GetStaticFieldOffset(void* klass, const char* fieldName);
void IL2CPP_DumpClasses();

#endif // IL2CPP_API_H
