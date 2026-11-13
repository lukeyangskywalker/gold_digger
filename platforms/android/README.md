[简体中文](README.zh_CN.md)

# Android app

A standard Gradle project (Java + NDK) that runs the real Passport UI, rules and
content through the shared runtime in [`platforms/common`](../common/README.md).

- `app/src/main/cpp/` builds `libspartan.so` with CMake: the shared runtime plus
  `spartan_jni.c`, the JNI bridge for `com.spartan.game.NativeBridge`.
- `GameView` (a `SurfaceView`) runs the game thread at ~60 Hz and draws the
  240x320 frame scaled with nearest-neighbour filtering. Input from the UI thread
  is queued onto the game thread (the native runtime is single-threaded).
- Controls: tap what you want on the screen (rows, camp stations, formation
  tiles, and the game's Back, Settings, speed and Report/Menu controls); rows
  with details take a second tap. Drag a list or its scroll bar to browse. The system Back goes back in
  the game and, on the camp, leaves the app. D-pad and keyboard keys still act
  as the Passport's Up/OK/Down. There are no on-screen buttons under the game.
- `AudioTrack` plays the game's 16 kHz music and effects.
- Saves: app-private `files/` (`save_v1.bin`, ...), same schema as the device,
  written atomically. The game saves on every pause and stops while the
  activity is in the background.
- App name and screen description are translated (`values-zh-rCN`); the game
  text uses the font compiled into the game.

## Layout

```
platforms/android/
  settings.gradle, build.gradle, gradle.properties
  gradle/wrapper/gradle-wrapper.properties   Gradle 8.7
  app/build.gradle                           AGP 8.5.2, compileSdk 34, minSdk 24, NDK 26.3, CMake 3.22.1
  app/src/main/AndroidManifest.xml
  app/src/main/java/com/spartan/game/        MainActivity, GameView, NativeBridge
  app/src/main/cpp/                          CMakeLists.txt, spartan_jni.c
  app/src/main/res/                          layout, values(-zh-rCN), mipmap-*/ic_launcher.png
```

`versionName` comes from the repository's `version.txt`.

## Build

Requirements: JDK 17, Android SDK (platform 34), NDK 26.3.11579264, CMake
3.22.1 (all installable from Android Studio's SDK Manager), and LVGL in
`managed_components/lvgl__lvgl` (fetched by any ESP-IDF build).

Open `platforms/android` in Android Studio, or from a shell:

```bash
cd platforms/android
gradle wrapper --gradle-version 8.7   # once: creates gradlew and gradle-wrapper.jar
./gradlew assembleDebug               # -> app/build/outputs/apk/debug/app-debug.apk
./gradlew installDebug                # to a connected device or emulator
```

The wrapper JAR is not checked in (it is a binary); Android Studio or the
`gradle wrapper` command generates it.
