[English](README.md)

# Android 应用

一个标准 Gradle 工程（Java + NDK），通过 [`platforms/common`](../common/README.zh_CN.md)
中的共享运行时运行真正的 Passport 界面、规则和内容。

- `app/src/main/cpp/` 用 CMake 构建 `libspartan.so`：共享运行时加上 `spartan_jni.c`，
  即 `com.spartan.game.NativeBridge` 的 JNI 桥。
- `GameView`（`SurfaceView`）以约 60 Hz 运行游戏线程，用最近邻缩放绘制 240x320 画面。
  UI 线程的输入排队后交给游戏线程处理（原生运行时是单线程的）。
- 操作：在屏幕上点按所需内容（列表行、营地地点、阵型格子，以及游戏内的返回、设置、速度和
  战报/菜单控件）；带详情的行需再次点按。拖动列表或其滚动条浏览。系统返回键在游戏内返回，在营地时退出应用。
  方向键和键盘仍对应 Passport 的上/确定/下。游戏下方不再有屏幕按钮。
- `AudioTrack` 播放游戏 16 kHz 的音乐和音效。
- 存档：应用私有的 `files/` 目录（`save_v1.bin` 等），与设备格式相同，原子写入。
  每次暂停时存档；应用在后台时游戏停止。
- 应用名称和画面说明已翻译（`values-zh-rCN`）；游戏文字使用游戏内置字体。

## 目录结构

```
platforms/android/
  settings.gradle、build.gradle、gradle.properties
  gradle/wrapper/gradle-wrapper.properties   Gradle 8.7
  app/build.gradle                           AGP 8.5.2、compileSdk 34、minSdk 24、NDK 26.3、CMake 3.22.1
  app/src/main/AndroidManifest.xml
  app/src/main/java/com/spartan/game/        MainActivity、GameView、NativeBridge
  app/src/main/cpp/                          CMakeLists.txt、spartan_jni.c
  app/src/main/res/                          layout、values(-zh-rCN)、mipmap-*/ic_launcher.png
```

`versionName` 取自仓库的 `version.txt`。

## 构建

需要：JDK 17、Android SDK（平台 34）、NDK 26.3.11579264、CMake 3.22.1
（均可通过 Android Studio 的 SDK Manager 安装），以及 `managed_components/lvgl__lvgl`
中的 LVGL（任何一次 ESP-IDF 构建都会下载）。

用 Android Studio 打开 `platforms/android`，或在命令行中：

```bash
cd platforms/android
gradle wrapper --gradle-version 8.7   # 只需一次：生成 gradlew 和 gradle-wrapper.jar
./gradlew assembleDebug               # -> app/build/outputs/apk/debug/app-debug.apk
./gradlew installDebug                # 安装到已连接的设备或模拟器
```

wrapper JAR 是二进制文件，没有放入仓库；Android Studio 或 `gradle wrapper` 命令会生成它。
