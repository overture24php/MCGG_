#pragma once
// ---------------------------------------------------------------------------
// Resolver IL2CPP by-name.
//
// Kenapa by-name dan bukan offset: dump runtime v1.2.98.3143 membuktikan nama
// class & signature Android IDENTIK dengan versi PC. Offset berubah tiap
// update, nama tidak.
//
// APK ini DIPAK: global-metadata.dat = 0 byte, libil2cpp.so = stub 0.37 MB,
// payload asli di libResources_z0_0..10. Jadi API il2cpp baru bisa dipakai
// SETELAH unpacker selesai -> Wait() melakukan polling, bukan asumsi siap.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstddef>

namespace il2 {

// ---- tipe opaque ----
using Domain   = void;
using Assembly = void;
using Image    = void;
using Class    = void;
using Method   = void;
using Field    = void;
using Object   = void;
using Thread   = void;

// ---- pointer API yang di-resolve lewat dlsym ----
struct Api {
    Domain*   (*domain_get)();
    Assembly** (*domain_get_assemblies)(Domain*, size_t*);
    Image*    (*assembly_get_image)(Assembly*);
    const char* (*image_get_name)(Image*);
    Class*    (*class_from_name)(Image*, const char* ns, const char* name);
    Method*   (*class_get_method_from_name)(Class*, const char* name, int argc);
    Field*    (*class_get_field_from_name)(Class*, const char* name);
    void      (*field_static_get_value)(Field*, void* out);
    void      (*field_static_set_value)(Field*, void* val);
    size_t    (*field_get_offset)(Field*);
    Thread*   (*thread_attach)(Domain*);
    void      (*thread_detach)(Thread*);
    const char* (*class_get_name)(Class*);
    void*     (*object_new)(Class*);
    void*     (*runtime_invoke)(Method*, void* obj, void** params, void** exc);
};

extern Api api;

// Tunggu sampai libil2cpp.so termuat DAN metadata siap (packer selesai).
// timeout_ms < 0 = tunggu selamanya. Return false kalau timeout.
bool Wait(int timeout_ms = 60000);

// image Assembly-CSharp.dll (tempat semua class game)
Image* CsImage();

// Cari class. ns boleh "" untuk global namespace.
// PENTING beda dari PC: Android pakai namespace eksplisit.
//   "Battle"    -> MCLogicHeroPool, AntiPluginComp, MCLogicReserveComp,
//                  MCLogicFightValueComp
//   "MTTDProto" -> Cmd_Battle_Result_CS, OperType_Battle_*
//   ""          -> MCLogicHeroShop, MCChessPlayerData, MCLogicBattleData,
//                  LogicChessManager, MCRelationManager, MCSystemData,
//                  BattleReceiveMessageBridge
Class* FindClass(const char* ns, const char* name);

// Alamat native method (untuk dipasang hook). argc -1 = abaikan jumlah arg.
void* MethodPtr(Class* k, const char* name, int argc = -1);
void* MethodPtr(const char* ns, const char* cls, const char* name, int argc = -1);

// offset field instance -> baca/tulis langsung lewat pointer objek
size_t FieldOffset(Class* k, const char* name);
size_t FieldOffset(const char* ns, const char* cls, const char* name);

// nilai field statis
bool StaticGet(const char* ns, const char* cls, const char* field, void* out);
bool StaticSet(const char* ns, const char* cls, const char* field, void* val);

// akses field instance dengan tipe
template <typename T>
inline T* FieldPtr(void* obj, size_t off) {
    return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(obj) + off);
}
template <typename T>
inline T FieldGet(void* obj, size_t off, T fallback = T{}) {
    if (!obj || off == 0) return fallback;
    return *FieldPtr<T>(obj, off);
}
template <typename T>
inline void FieldSet(void* obj, size_t off, T v) {
    if (!obj || off == 0) return;
    *FieldPtr<T>(obj, off) = v;
}

// buat objek + panggil .ctor (untuk OperType_Battle_* saat kirim op)
void* NewObject(const char* ns, const char* cls);

// thread saat ini harus di-attach ke domain sebelum menyentuh objek managed
struct ScopedThread {
    Thread* t = nullptr;
    ScopedThread();
    ~ScopedThread();
};

} // namespace il2
