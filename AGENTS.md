# Gold Digger development

Same architecture as `../Spartan`. Keep game rules in portable C under `core/`
and hardware/LVGL/NVS in `platforms/passport/`. The Passport UI
(`platforms/passport/main/ui/`) is the only UI. Host ports reuse it through
`platforms/common/` and never fork game logic. Current focus: the Passport.

ESP32-C3, 8 MiB flash, no PSRAM, ESP-IDF 5.5.3. Queue button callbacks; never
block in them. Lock LVGL outside its task. Never erase NVS automatically after an error.
Keep saves versioned (`GD_SCHEMA`) and reject newer ones. Keep maintained
Markdown paired in English and Simplified Chinese.

Checks:
- `python tools/run_host_tests.py`: core tests plus the headless UI smoke test,
  which renders the real LVGL UI at 240x320 and writes screenshots to `build/shots/`.
- `tools/build-passport.ps1` in an activated IDF environment.

Do not flash, erase, commit or publish without explicit authorization. A
successful build or a headless screenshot is not device validation.
