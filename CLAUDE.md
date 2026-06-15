# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Flash Commands

All commands run via **idf.py** inside the ESP-IDF terminal (the IDF environment must be sourced first):

```sh
idf.py build                          # compile
idf.py flash                          # flash to connected ESP32
idf.py monitor                        # open serial monitor (Ctrl+] to exit)
idf.py flash monitor                  # flash then immediately open monitor
idf.py fullclean && idf.py build      # clean rebuild (use when changing sdkconfig)
idf.py erase-flash                    # wipe NVS and all partitions before reflash
```

JTAG debugging (run as admin):
```sh
idf.py openocd --openocd-commands "-f board/esp32-bridge.cfg"
# then attach VS Code debugger via Run & Debug (Fn+F5)
```

There are no unit tests.

## Project Architecture

This is an **ESP-IDF (v5.x) FreeRTOS firmware** for an ESP32-based alarm clock with a 128x64 SSD1309 OLED, two KY-040 rotary encoders, and a YX5200 DFPlayer MP3 module.

### Task Structure (`main/src/main.c`)

`app_main` initializes hardware then spawns these FreeRTOS tasks:

| Task | Priority | Purpose |
|---|---|---|
| `splash_load_task` | 5 | Drives animated progress bar during boot |
| `display_task` | 4 | Redraws OLED at ~20 Hz |
| `alarm_task` | 5 | Polls alarm state machine and time |
| `song_task` | 6 | Handles songs-encoder button to play/stop |
| `rotary_task` | 7 | Polls both PCNT encoders, triggers display wakeup |

Task coordination uses **direct-to-task notifications** (`xTaskNotifyGive` / `ulTaskNotifyTake`), not semaphores.

### Display Layer (`main/src/ssd1309.c`, `main/inc/ssd1309.h`)

- Single 1 KB frame buffer (`oled_buffer[1024]`) written all at once via I2C on `ssd1309_display()`.
- **Page-based layout**: y parameter in draw calls is a *page* (0-7), not a pixel row. Each page = 8 vertical pixels.
- `ssd1309_draw_text(x_pixel, page, str)` — x is pixel column, y is page.
- `ssd1309_draw_xbm(x_pixel, page, w, h, bitmap)` — renders XBM bitmaps stored in `graphics_bitmaps.c`.
- Display auto-dims (`DISPLAY_DIM`) after 2 s idle and turns off (`DISPLAY_OFF`) after 5 s. Call `screen_activity()` to reset the timer.
- Brightness control via I2C commands to VCOMH and contrast registers (see tunables block in ssd1309.c).

### Alarm State Machine (`main/src/main.c`)

```
ALARM_IDLE → (encoder button) → ALARM_CONFIG → (encoder button) → ALARM_ARMED
ALARM_ARMED → (time match) → ALARM_TRIGGERED → (encoder button) → ALARM_IDLE
```

Alarm time is stored as an index (`index_alarm`): minutes = `index_alarm * 5 % 60`, hour = `index_alarm * 5 / 60`. Full range is 0-287 (288 x 5 min = 24 h). The alarm rotary only counts when `alarm == ALARM_CONFIG`.

### Peripheral Drivers

- **Rotary encoders** (`rotary.c`): Use ESP32 PCNT (pulse counter) hardware. `rotary_index()` converts raw count delta to a bounded array index.
- **MP3** (`mp3.c`): UART to DFPlayer Mini (YX5200). Commands via `mp3_cmd(CMD_*, value)`. GPIO 4 controls module power via transistor; GPIO 17 is TX.
- **WiFi** (`wifi.c`): STA mode, event-driven. Connects on boot for SNTP time sync. Credentials stored in `passwords.h` (not committed). Max 5 retries.

### Graphics Assets

- Small font bitmaps: `graphics_smallfonts.c/.h`
- Large font bitmaps: `graphics_bigfonts.c/.h`
- XBM images (icons, splash clock, progress bar): `graphics_bitmaps.c`

## Key Files

| File | Role |
|---|---|
| `main/src/main.c` | Entry point, all task definitions, alarm & rotary logic |
| `main/src/ssd1309.c` | OLED driver + display state (dim/off) + all draw primitives |
| `main/src/wifi.c` | WiFi STA init + SNTP time sync |
| `main/src/mp3.c` | DFPlayer UART driver |
| `main/src/rotary.c` | PCNT-based rotary encoder driver |
| `main/inc/ssd1309.h` | `enum brightness`, `enum display_screen`, all draw function signatures |
| `main/inc/mp3.h` | MP3 command constants (`CMD_*`) |
| `passwords.h` | WiFi SSID/password — **not in version control** |

## Hardware Notes

- Display is electrically a **SSD1309** but behaves like a SSD1306 (same I2C protocol, address `0x3C`).
- Alarm rotary SW = GPIO 36 (input-only pin); Songs rotary SW = GPIO 39 (input-only pin). Both are active-low, polled directly with `gpio_get_level`.
- `sdkconfig.defaults` is the source of truth for SDK config; never edit `sdkconfig` directly.
