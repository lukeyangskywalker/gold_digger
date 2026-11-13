/* JNI bridge between com.spartan.game.NativeBridge and the shared Spartan host
   runtime (platforms/common). All calls except audio() come from the Java game
   thread; audio() comes from the AudioTrack thread and is internally guarded. */
#define _POSIX_C_SOURCE 200809L /* clock_gettime */
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "spartan_host.h"
#include "storage_files.h"

static SpFileStorage storage;
static int started; /* LVGL is initialised once per process; activities may be recreated */
static AAssetManager *assets;
static jobject assets_ref; /* keeps the Java AssetManager (and so `assets`) alive */

/* Soundtrack loops are APK assets music/<name>.spm (stored uncompressed, see
   build.gradle). The asset stays open so its buffer stays valid. */
static void request_music(void *user, const char *name) {
    (void)user;
    if (!assets) return;
    char path[96];
    snprintf(path, sizeof(path), "music/%s.spm", name);
    AAsset *asset = AAssetManager_open(assets, path, AASSET_MODE_BUFFER);
    if (!asset) return;
    const void *data = AAsset_getBuffer(asset);
    if (data)
        spartan_host_music_loaded(name, data, (size_t)AAsset_getLength(asset));
    else
        AAsset_close(asset);
}

static int64_t now_us(void *user) {
    (void)user;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

JNIEXPORT jboolean JNICALL Java_com_spartan_game_NativeBridge_init(JNIEnv *env, jclass cls, jstring dir,
                                                                   jint seed, jobject asset_manager) {
    (void)cls;
    if (started) return JNI_TRUE;
    if (asset_manager && !assets_ref) {
        assets_ref = (*env)->NewGlobalRef(env, asset_manager);
        assets = AAssetManager_fromJava(env, assets_ref);
    }
    const char *path = (*env)->GetStringUTFChars(env, dir, NULL);
    if (!path) return JNI_FALSE;
    sp_file_storage_init(&storage, path);
    (*env)->ReleaseStringUTFChars(env, dir, path);
    SpartanHostConfig config = {sp_file_storage_read, sp_file_storage_write, now_us,       &storage,
                                (uint32_t)seed,       SPARTAN_PIXELS_ARGB,   request_music};
    started = spartan_host_init(&config);
    return started ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jboolean JNICALL Java_com_spartan_game_NativeBridge_tick(JNIEnv *env, jclass cls) {
    (void)env;
    (void)cls;
    return started && spartan_host_tick() ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL Java_com_spartan_game_NativeBridge_key(JNIEnv *env, jclass cls, jint key,
                                                              jboolean down) {
    (void)env;
    (void)cls;
    if (started) spartan_host_key((SpartanKey)key, down == JNI_TRUE);
}
JNIEXPORT void JNICALL Java_com_spartan_game_NativeBridge_keyCancel(JNIEnv *env, jclass cls, jint key) {
    (void)env;
    (void)cls;
    if (started) spartan_host_key_cancel((SpartanKey)key);
}
JNIEXPORT void JNICALL Java_com_spartan_game_NativeBridge_pointer(JNIEnv *env, jclass cls, jint phase, jint x,
                                                                  jint y) {
    (void)env;
    (void)cls;
    if (started) spartan_host_pointer((SpartanPointer)phase, x, y);
}
JNIEXPORT jboolean JNICALL Java_com_spartan_game_NativeBridge_back(JNIEnv *env, jclass cls) {
    (void)env;
    (void)cls;
    return started && spartan_host_back() ? JNI_TRUE : JNI_FALSE;
}
/* Copies the 240x320 ARGB frame into pixels (length >= 76800). */
JNIEXPORT void JNICALL Java_com_spartan_game_NativeBridge_copyFrame(JNIEnv *env, jclass cls,
                                                                    jintArray pixels) {
    (void)cls;
    if (!started || (*env)->GetArrayLength(env, pixels) < SPARTAN_SCREEN_W * SPARTAN_SCREEN_H) return;
    (*env)->SetIntArrayRegion(env, pixels, 0, SPARTAN_SCREEN_W * SPARTAN_SCREEN_H,
                              (const jint *)spartan_host_framebuffer());
}
JNIEXPORT jint JNICALL Java_com_spartan_game_NativeBridge_backlight(JNIEnv *env, jclass cls) {
    (void)env;
    (void)cls;
    return (jint)spartan_host_backlight();
}
JNIEXPORT void JNICALL Java_com_spartan_game_NativeBridge_save(JNIEnv *env, jclass cls) {
    (void)env;
    (void)cls;
    if (started) spartan_host_save();
}
JNIEXPORT void JNICALL Java_com_spartan_game_NativeBridge_audio(JNIEnv *env, jclass cls, jshortArray pcm,
                                                                jint count) {
    (void)cls;
    int16_t buffer[1024];
    jint length = (*env)->GetArrayLength(env, pcm);
    if (count > length) count = length;
    for (jint at = 0; at < count;) {
        jint n = count - at < 1024 ? count - at : 1024;
        spartan_host_audio_render(buffer, (size_t)n);
        (*env)->SetShortArrayRegion(env, pcm, at, n, buffer);
        at += n;
    }
}
JNIEXPORT jstring JNICALL Java_com_spartan_game_NativeBridge_version(JNIEnv *env, jclass cls) {
    (void)cls;
    return (*env)->NewStringUTF(env, spartan_host_version());
}
