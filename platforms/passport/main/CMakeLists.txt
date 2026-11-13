# Passport firmware component: the UI modules in ui/ plus the portable core.
# ESP-IDF evaluates this file in script mode, so the glob cannot use
# CONFIGURE_DEPENDS: run idf.py reconfigure after adding or removing a module.
file(GLOB ui_sources "${CMAKE_CURRENT_LIST_DIR}/ui/*.c")

idf_component_register(
    SRCS ${ui_sources} "storage.c" "game_audio.c" "gd_synth.c" "../../../core/digger.c"
    INCLUDE_DIRS "." "../../../core"
    REQUIRES bsp nvs_flash esp_partition esp_timer esp_hw_support
    PRIV_REQUIRES esp_app_format
)
