#include "include/il2cpp.h"
#include <dlfcn.h>
#include <cstring>

namespace IL2CPP {

il2cpp_domain_get_t il2cpp_domain_get = nullptr;
il2cpp_domain_get_assemblies_t il2cpp_domain_get_assemblies = nullptr;
il2cpp_assembly_get_image_t il2cpp_assembly_get_image = nullptr;
il2cpp_image_get_name_t il2cpp_image_get_name = nullptr;
il2cpp_class_from_name_t il2cpp_class_from_name = nullptr;
il2cpp_class_get_method_from_name_t il2cpp_class_get_method_from_name = nullptr;
il2cpp_class_get_field_from_name_t il2cpp_class_get_field_from_name = nullptr;
il2cpp_field_get_offset_t il2cpp_field_get_offset = nullptr;
il2cpp_field_static_get_value_t il2cpp_field_static_get_value = nullptr;
il2cpp_field_static_set_value_t il2cpp_field_static_set_value = nullptr;
il2cpp_object_new_t il2cpp_object_new = nullptr;
il2cpp_runtime_invoke_t il2cpp_runtime_invoke = nullptr;
il2cpp_thread_attach_t il2cpp_thread_attach = nullptr;
il2cpp_thread_detach_t il2cpp_thread_detach = nullptr;
il2cpp_class_get_name_t il2cpp_class_get_name = nullptr;
il2cpp_image_get_class_count_t il2cpp_image_get_class_count = nullptr;
il2cpp_image_get_class_t il2cpp_image_get_class = nullptr;

void Init(uintptr_t baseAddress) {
    void* handle = dlopen("libil2cpp.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) return;

    il2cpp_domain_get = (il2cpp_domain_get_t)dlsym(handle, "il2cpp_domain_get");
    il2cpp_domain_get_assemblies = (il2cpp_domain_get_assemblies_t)dlsym(handle, "il2cpp_domain_get_assemblies");
    il2cpp_assembly_get_image = (il2cpp_assembly_get_image_t)dlsym(handle, "il2cpp_assembly_get_image");
    il2cpp_image_get_name = (il2cpp_image_get_name_t)dlsym(handle, "il2cpp_image_get_name");
    il2cpp_class_from_name = (il2cpp_class_from_name_t)dlsym(handle, "il2cpp_class_from_name");
    il2cpp_class_get_method_from_name = (il2cpp_class_get_method_from_name_t)dlsym(handle, "il2cpp_class_get_method_from_name");
    il2cpp_class_get_field_from_name = (il2cpp_class_get_field_from_name_t)dlsym(handle, "il2cpp_class_get_field_from_name");
    il2cpp_field_get_offset = (il2cpp_field_get_offset_t)dlsym(handle, "il2cpp_field_get_offset");
    il2cpp_field_static_get_value = (il2cpp_field_static_get_value_t)dlsym(handle, "il2cpp_field_static_get_value");
    il2cpp_field_static_set_value = (il2cpp_field_static_set_value_t)dlsym(handle, "il2cpp_field_static_set_value");
    il2cpp_object_new = (il2cpp_object_new_t)dlsym(handle, "il2cpp_object_new");
    il2cpp_runtime_invoke = (il2cpp_runtime_invoke_t)dlsym(handle, "il2cpp_runtime_invoke");
    il2cpp_thread_attach = (il2cpp_thread_attach_t)dlsym(handle, "il2cpp_thread_attach");
    il2cpp_thread_detach = (il2cpp_thread_detach_t)dlsym(handle, "il2cpp_thread_detach");
    il2cpp_class_get_name = (il2cpp_class_get_name_t)dlsym(handle, "il2cpp_class_get_name");
    il2cpp_image_get_class_count = (il2cpp_image_get_class_count_t)dlsym(handle, "il2cpp_image_get_class_count");
    il2cpp_image_get_class = (il2cpp_image_get_class_t)dlsym(handle, "il2cpp_image_get_class");
}

void* FindClass(const char* ns, const char* name) {
    if (!il2cpp_domain_get || !il2cpp_class_from_name) return nullptr;
    
    auto domain = il2cpp_domain_get();
    if (!domain) return nullptr;
    
    size_t n = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &n);
    if (!assemblies || n == 0) return nullptr;
    
    for (size_t i = 0; i < n; i++) {
        auto img = il2cpp_assembly_get_image(assemblies[i]);
        if (!img) continue;
        
        auto klass = il2cpp_class_from_name(img, ns, name);
        if (klass) return klass;
    }
    return nullptr;
}

void* GetMethod(void* klass, const char* name, int argc) {
    if (!klass || !il2cpp_class_get_method_from_name) return nullptr;
    return il2cpp_class_get_method_from_name(klass, name, argc);
}

size_t GetFieldOffset(void* klass, const char* name) {
    if (!klass || !il2cpp_class_get_field_from_name || !il2cpp_field_get_offset) return 0;
    auto field = il2cpp_class_get_field_from_name(klass, name);
    if (!field) return 0;
    return il2cpp_field_get_offset(field);
}

void* NewObject(const char* ns, const char* cls) {
    auto klass = FindClass(ns, cls);
    if (!klass || !il2cpp_object_new) return nullptr;
    return il2cpp_object_new(klass);
}

void* InvokeMethod(void* method, void* obj, void** params) {
    if (!method || !il2cpp_runtime_invoke) return nullptr;
    void* exc = nullptr;
    auto result = il2cpp_runtime_invoke(method, obj, params, &exc);
    if (exc) return nullptr;
    return result;
}

} // namespace IL2CPP
