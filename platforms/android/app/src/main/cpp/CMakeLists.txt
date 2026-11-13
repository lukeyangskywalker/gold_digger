# Native side of the Android app: the shared Spartan host runtime (core,
# content, Passport UI, LVGL) plus a thin JNI bridge.
cmake_minimum_required(VERSION 3.22.1)
project(spartan_android C)
set(CMAKE_C_STANDARD 11)

get_filename_component(SPARTAN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../.." ABSOLUTE)
add_subdirectory("${SPARTAN_ROOT}/platforms/common" spartan_common)

add_library(spartan SHARED spartan_jni.c)
target_link_libraries(spartan PRIVATE spartan_host log android) # android: AAssetManager (soundtrack)
