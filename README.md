# ESP32 Smart Alarm Clock

A modular embedded alarm clock implemented in C with ESP-IDF and FreeRTOS. The system combines deterministic alarm control, two rotary quadrature encoder inputs, OLED graphics on a 128x64p display (SSD1309), MP3 playback, Wi-Fi time synchronization, and radar-based snooze detection on an ESP32.

## Project Status

Active development. Core alarm, display, rotary input, audio playback, screen-dimming, and network time-synchronization features are functional.

Radar snoozing still in testing

## Key Features

- Configurable alarm with explicit application states:
  - `ALARM_IDLE`
  - `ALARM_CONFIG_HOUR`
  - `ALARM_CONFIG_MINUTE`
  - `ALARM_ARMED`
  - `ALARM_TRIGGERED`
  - `ALARM_SNOOZED`
- Left-hand Rotary Encoder Switch for song selection and start/stopping MP3 playback
- Right-hand Rotary Encoder Switch for alarm hour and minute configuration, as well as enabling the alarm
- ESP32 PCNT-based quadrature decoding for fast and accurate scrolling, while also preventing the need for extra input polling
- SSD1309 128 × 64 monochrome OLED with framebuffer rendering
- Configurable display brightness, dimming, and automatic shutoff of the display depending on Alarm state.
- MP3 playback through a YX5200 (third party)/DFPlayer-compatible module with micro SD card
- LD2410C radar input for presence-based snooze behavior
- Wi-Fi connection and SNTP time synchronization
- JTAG debugging with ESP-PROG-2, OpenOCD, and GDB
- FreeRTOS task notifications and periodic task execution

## Firmware Architecture

The firmware is divided by responsibility rather than implemented as a single monolithic control loop.

 Module Breakdown

 `main.c`  Application startup, task creation, shared application state, and system-level orchestration 

 `display_ssd1309.c`  OLED initialization, framebuffer management

 `display_graphics.c` drawing primitives and formatting 
 
 `display_application.c`display refresh, screen-saver behavior, and general features

 `rotary.c`  Rotary encoder initialization, PCNT count processing, direction handling, index updates, and button input 

 `mp3.c`  UART communication with the MP3 module, playback control, and amplifier/module control 

 `wifi.c`  Wi-Fi initialization and SNTP time synchronization 

 `songs.c`  Song metadata and track-selection support 

### State-Driven Design

Alarm behavior is represented by a finite-state machine instead of loosely coupled flags. This keeps configuration, armed, triggered, and snoozed behavior mutually understandable and easier to extend.

The display renders from the current alarm state rather than owning alarm behavior. This separation prevents the UI task from becoming the authority for application control.

### FreeRTOS Responsibilities

The project uses multiple FreeRTOS execution contexts for independent responsibilities such as:

- display rendering and refresh
- rotary input processing
- alarm-state evaluation
- audio control
- network time synchronization
- radar-based snooze detection

Task notifications are used for lightweight event signaling where appropriate. For example, rotary activity can notify the display task to reset its inactivity timer without requiring continuous cross-task polling.

## Peripheral Interfaces

| Peripheral | Interface | ESP32 Assignment |
|---|---|---|
| SSD1309 OLED | I2C | SDA: GPIO21, SCL: GPIO22, RESET: GPIO23 |
| LD2410C radar | UART receive | GPIO1 |
| YX5200 MP3 module | UART transmit/control | GPIO17 |
| MP3/amp control transistor | GPIO | GPIO4 |
| Alarm encoder | PCNT + GPIO | CLK: GPIO25, DATA: GPIO26, SW: GPIO36 |
| Music encoder | PCNT + GPIO | CLK: GPIO32, DATA: GPIO33, SW: GPIO39 |

### Reserved JTAG Pins

- Signal     ESP32 Pin 
- TCK         GPIO13 
- TMS         GPIO14 
- TDI         GPIO12 
- TDO         GPIO15 
- GND         GND 
- Target reference  3.3 V 

## Display Driver Notes

The SSD1309 display is organized as 128 columns by 8 pages. Each page contains 8 vertical pixels per byte.

- Resolution: 128 × 64
- I2C address: `0x3C`
- Command-control byte: `0x00`
- Display-data control byte: `0x40`
- Page addressing: `B0` through `B7`
- The controller automatically increments the column address while display data is written

The driver maintains a framebuffer and performs full or controlled display updates over I2C. Application-level drawing code writes into the framebuffer before `ssd1309_display()` transfers the completed frame.

## Rotary Encoder Processing

Both rotary encoders use the ESP32 pulse counter peripheral rather than relying only on software polling.

The PCNT peripheral captures quadrature transitions, while firmware converts count changes into application-level index updates. Alarm configuration and music browsing can use different indexing policies so that precise time entry does not require the same response profile as navigating a song list.

## Alarm and Snooze Behavior

The alarm state machine controls transitions among configuration, armed, triggered, and snoozed operation.

When the alarm is triggered:

1. The selected audio track is started.
2. The display changes to the triggered presentation.
3. Presence information from the LD2410C can request a snooze.
4. Snooze logic stops playback and schedules the next trigger.
5. A user acknowledgement returns the system to its non-triggered state.

## Time Synchronization

The ESP32 connects to Wi-Fi and initializes system time through SNTP. After synchronization, the alarm and display logic use the ESP32 system clock rather than repeatedly coupling UI behavior to the network layer.

Network time and local runtime behavior are intentionally separated so that normal alarm-clock operation does not depend on continuous Wi-Fi traffic.

## Build and Flash

### Prerequisites

- ESP-IDF 5.5
- ESP32 toolchain
- CMake and Ninja as installed by ESP-IDF
- USB serial connection to the target

### Build

idf.py set-target esp32
idf.py build

### Flash and Monitor

idf.py -p COMx flash monitor

### Clean Rebuild

idf.py fullclean
idf.py build


### Erase and Reflash

idf.py erase-flash
idf.py flash
idf.py monitor


## JTAG Debugging

The project supports source-level debugging through ESP-PROG-2, OpenOCD, and the ESP-IDF GDB toolchain.

Typical workflow:

1. Connect ESP-PROG-2 to the reserved JTAG pins.
2. Start OpenOCD from an ESP-IDF terminal.
3. Launch the VS Code debug configuration.
4. Set breakpoints, inspect task state, and step through firmware.
5. Use the OpenOCD console for reset and halt commands when required.

Example monitor command:

monitor reset halt

On Windows, the ESP-PROG-2 interface may require the WinUSB driver. I used Zadig to install the correct ones, which was WIN-USB-32


## Planned Improvements

- White Noise Feature
- Volume ramping during wakeup alarm
- DFPlayer acknowledgement (RX)
- Hand-Wave song change feature
- Persistent timekeeping with a DS3231 RTC
- Additional separation between UI rendering and SSD1309 driver internals

## Hardware

Current major components:

- ESP32 ESP-WROOM-32 development board
- SSD1309 128 × 64 OLED
- Two KY-040 rotary encoders
- "YX5200"/DFPlayer-compatible MP3 module
    NOTE: the breakout board is a YX5200 clone and uses
            the same commands on the YX5200 datasheet
- PAM8406 audio amplifier
- 3 W, 4 Ω speakers
- LD2410C radar sensor

