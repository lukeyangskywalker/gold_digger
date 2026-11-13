package com.spartan.game;

/** JNI surface of libspartan.so (app/src/main/cpp/spartan_jni.c). */
final class NativeBridge {
    static final int KEY_UP = 0;
    static final int KEY_DOWN = 1;
    static final int KEY_OK = 2;
    static final int POINTER_DOWN = 0;
    static final int POINTER_MOVE = 1;
    static final int POINTER_UP = 2;
    static final int POINTER_CANCEL = 3;
    static final int SCREEN_W = 240;
    static final int SCREEN_H = 320;
    static final int AUDIO_RATE_HZ = 32000;

    static {
        System.loadLibrary("spartan");
    }

    private NativeBridge() {}

    /** Loads the save from {@code dataDir} and builds the UI; safe to call again. The
     *  AssetManager supplies the soundtrack (assets/music/*.spm). */
    static native boolean init(String dataDir, int seed, android.content.res.AssetManager assets);
    /** One Passport main-loop iteration; true when the frame changed. */
    static native boolean tick();
    static native void key(int key, boolean down);
    static native void keyCancel(int key);
    /** Touch in screen pixels (0..239, 0..319): taps choose, drags scroll. */
    static native void pointer(int phase, int x, int y);
    /** The game's Back; false on the camp, which has nothing to return to. */
    static native boolean back();
    static native void copyFrame(int[] argb);
    static native int backlight();
    static native void save();
    /** Thread-safe: fills {@code count} mono 16 kHz samples. */
    static native void audio(short[] pcm, int count);
    static native String version();
}
