[简体中文](README.zh_CN.md)

# Gold Digger

An original mining roguelite for the FoloToy AI Passport (ESP32-C3, 240x320,
three buttons). It is built the same way as Spartan: the game rules are
portable C in `core/`, and the Passport LVGL UI in `platforms/passport/main/ui/`
is the only UI. Desktop and Android ports will reuse that UI through the host
shims in `platforms/common/`. Gold is the only currency.

## How to play

Dive into a fresh mine that is 8 columns wide and 400 m deep. Dig left, right or down,
collect ore, then rope back up to sell it for gold.

- **Energy:** every swing of the pickaxe costs 1 energy. At zero energy you
  faint; you are rescued but lose half your bag.
- **Layers:** dirt, clay, stone, granite, basalt and obsidian take 1 to 6 hits
  with the starting pickaxe. Each pickaxe level takes one hit off every layer,
  down to a minimum of 1. A tile you are working on shows cracks. Bedrock and
  lava can never be dug and are marked with a red ✕.
- **Ore:** coal, copper, silver, gold, ruby and diamond, which get rarer and
  more valuable with depth. Treasure chests pay gold directly. Two relic chests
  rest on the bedrock.
- **Hazards:** gas pockets drain energy and are only visible with a level 3
  lantern. Lava sits on cave floors below 110 m. You cannot step beside it, but
  digging into a cave can drop you into it, and then the whole bag is lost.
  Falls of more than 3 m cost energy.
- **Trading Post:** Pickaxe (fewer hits), Lantern (light radius, gas at level 3), Stamina (energy), Ore
  bag (capacity), Lucky charm (+5% ore value per level) and Dynamite (clears
  the 3x3 around you and collects the ore).
- **Elevator:** a stop every 50 m. Reaching a depth lets you buy its stop once,
  and later dives can start there.

## Passport controls

| Where | UP | DOWN | OK | Hold OK |
| --- | --- | --- | --- | --- |
| Camp / menus | previous | next | choose | back |
| Mine | dig/walk left | dig/walk right | dig down | pause (dynamite, rope up) |

Holding UP or DOWN repeats the move. The game saves after every dive and
purchase, and every 20 s while you are in the mine. When idle it dims the screen,
then saves and deep-sleeps (Settings → Sleep when idle). Any button wakes it,
and the press that wakes it is ignored. While USB power is present the screen
turns off instead of sleeping.

## Layout

```
core/                    rules, generation, save format (pure C11)
tests/core_tests.c       host tests for core/
platforms/passport/      firmware: BSP component, main/ (storage, audio, ui/)
platforms/common/        host runtime and ESP-IDF/BSP shims (headless smoke test today)
```

## Build

Host tests:

```bash
python tools/run_host_tests.py
```

Firmware (ESP-IDF 5.5.3, activated):

```powershell
tools/build-passport.ps1
```

This produces `build/passport/GoldDigger.bin` and the merged `GoldDigger-full.bin`.
Flashing needs your go-ahead (port and data impact) and is not part of the build.
