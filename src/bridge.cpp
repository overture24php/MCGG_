// ---------------------------------------------------------------------------
// JNI bridge untuk menu in-process (ph/over/mcgg/MCGG.java di APK repack).
// Zygisk path tidak memanggil file ini — natives hanya aktif saat Java memuat
// .so via System.loadLibrary. JNIEXPORT sudah membawa visibility("default")
// jadi -fvisibility=hidden tidak menyembunyikan simbol ini.
// ---------------------------------------------------------------------------
#include <jni.h>
#include <cstring>

#include "config.h"
#include "log.h"

static const char* JStr(JNIEnv* env, jstring js) {
    if (!js) return nullptr;
    return env->GetStringUTFChars(js, nullptr);
}

extern "C" {

JNIEXPORT void JNICALL
Java_ph_over_mcgg_MCGG_setPersist(JNIEnv* env, jclass, jstring jkey, jboolean on) {
    const char* k = JStr(env, jkey);
    if (!k) return;
    cfg::SetPersist(k, on != JNI_FALSE);
    env->ReleaseStringUTFChars(jkey, k);
}

JNIEXPORT jboolean JNICALL
Java_ph_over_mcgg_MCGG_getPersist(JNIEnv* env, jclass, jstring jkey) {
    const char* k = JStr(env, jkey);
    if (!k) return JNI_FALSE;
    bool v = cfg::GetPersist(k);
    env->ReleaseStringUTFChars(jkey, k);
    return v ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_ph_over_mcgg_MCGG_trigger(JNIEnv* env, jclass, jstring jkey) {
    const char* k = JStr(env, jkey);
    if (!k) return;
    cfg::TriggerOnce(k);
    env->ReleaseStringUTFChars(jkey, k);
}

} // extern "C"