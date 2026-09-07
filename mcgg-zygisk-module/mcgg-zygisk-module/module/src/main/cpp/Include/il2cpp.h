#ifndef MCGG_IL2CPP_H
#define MCGG_IL2CPP_H

#include <cstdint>
#include <cstddef>

namespace IL2CPP {

// IL2CPP function signatures
typedef void* (*il2cpp_domain_get_t)();
typedef void** (*il2cpp_domain_get_assemblies_t)(void* domain, size_t* size);
typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
typedef const char* (*il2cpp_image_get_name_t)(void* image);
typedef void* (*il2cpp_class_from_name_t)(void* image, const char* ns, const char* name);
typedef void* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int argsCount);
typedef void* (*il2cpp_class_get_field_from_name_t)(void* klass, const char* name);
typedef size_t (*il2cpp_field_get_offset_t)(void* field);
typedef void (*il2cpp_field_static_get_value_t)(void* field, void* value);
typedef void (*il2cpp_field_static_set_value_t)(void* field, void* value);
typedef void* (*il2cpp_object_new_t)(void* klass);
typedef void* (*il2cpp_runtime_invoke_t)(void* method, void* obj, void** params, void** exc);
typedef void* (*il2cpp_thread_attach_t)(void* domain);
typedef void (*il2cpp_thread_detach_t)(void* thread);
typedef const char* (*il2cpp_class_get_name_t)(void* klass);
typedef uint32_t (*il2cpp_image_get_class_count_t)(void* image);
typedef void* (*il2cpp_image_get_class_t)(void* image, uint32_t index);

// Global function pointers
extern il2cpp_domain_get_t il2cpp_domain_get;
extern il2cpp_domain_get_assemblies_t il2cpp_domain_get_assemblies;
extern il2cpp_assembly_get_image_t il2cpp_assembly_get_image;
extern il2cpp_image_get_name_t il2cpp_image_get_name;
extern il2cpp_class_from_name_t il2cpp_class_from_name;
extern il2cpp_class_get_method_from_name_t il2cpp_class_get_method_from_name;
extern il2cpp_class_get_field_from_name_t il2cpp_class_get_field_from_name;
extern il2cpp_field_get_offset_t il2cpp_field_get_offset;
extern il2cpp_field_static_get_value_t il2cpp_field_static_get_value;
extern il2cpp_field_static_set_value_t il2cpp_field_static_set_value;
extern il2cpp_object_new_t il2cpp_object_new;
extern il2cpp_runtime_invoke_t il2cpp_runtime_invoke;
extern il2cpp_thread_attach_t il2cpp_thread_attach;
extern il2cpp_thread_detach_t il2cpp_thread_detach;
extern il2cpp_class_get_name_t il2cpp_class_get_name;
extern il2cpp_image_get_class_count_t il2cpp_image_get_class_count;
extern il2cpp_image_get_class_t il2cpp_image_get_class;

// Initialize IL2CPP function pointers from base address
void Init(uintptr_t baseAddress);

// Helper: find class by name
void* FindClass(const char* ns, const char* name);

// Helper: get method pointer
void* GetMethod(void* klass, const char* name, int argc = -1);

// Helper: get field offset
size_t GetFieldOffset(void* klass, const char* name);

// Helper: create new object
void* NewObject(const char* ns, const char* cls);

// Helper: invoke method
void* InvokeMethod(void* method, void* obj, void** params = nullptr);

} // namespace IL2CPP

#endif
