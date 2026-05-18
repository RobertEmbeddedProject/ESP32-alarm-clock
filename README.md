
# ESP32 Alarm Clock Project

## Hardware
### Components
- SSD1306 OLED display
- KY-040	Rotary Encoders
- YX5200 	MP3 Module
- PAM8406	Amplifier
- Speaker 3W 4ohm Speakers (generic)
            5-15W Consumption 88dB, 108Hz~18KHz±3dB
- LD2410C Radar Sensor

**Pontentially Implement Later**
- LM2596   Buck Converter        (24v->5v)
- TXS0108E Logic Level Shifter (5v->3.3v)
- DS3231   Clock Preservation Module

### IO Assignment
- GPIO 1  = Radar Snooze Consumed Data (UART)
- GPIO 4  = MP3 Module Power (Transistor Circuit)
- GPIO 17 = MP3 Module Produced Data   (UART)
- GPIO 21 = SSD1309 SDA (I2C)
- GPIO 22 = SSD1309 SCL (I2C)
- GPIO 23 = SSD1309 RESET
- GPIO 36 = Alarm SW (vp)
- GPIO 25 = Alarm CLK
- GPIO 26 = Alarm DATA
- GPIO 32 = Songs CLK
- GPIO 33 = Songs DATA
- GPIO 39 = Songs SW (vn)

**Reserved for Debugging**
- JTAG TCK  -> ESP32 GPIO13
- JTAG TMS  -> ESP32 GPIO14
- JTAG TDI  -> ESP32 GPIO12
- JTAG TDO  -> ESP32 GPIO15
- JTAG GND  -> ESP32 GND
- JTAG 3V3 sense/reference -> ESP32 3V3

### Firmware Notes
**SSD1309 Display**
- 128 x 64 pixels
- 128 columns by 8 pages (1 page = 8 bits)
- Each byte sent represents one vertical column of 8 pixels, starting from the bottom of the byte
- Over I2C, you select page (B0...B7) and the column (C0...C127)
    - SSD1309 automatically updates column after each byte, so a full display refresh simplifies transactions
- Use 0x3C for device address
- 0x00 sets display to accept commands, 0x40 sets display to accept incoming pixel data

### JTAG Debugging Setup
Initial Windows Setup
- With the ESP-PROG-2, may need to use Zadig software to change
    driver to WinUSB

**Using the Debugger**
Open Terminal App as admin, and in ESP-IDF tab:
- cd "Project directory"
- idf.py openocd --openocd-commands "-f board/esp32-bridge.cfg"
- OpenOCD is now running in the background.

In VScode:
- CTRL+SHFT+D (or Run and Debug)
- Press Fn+F5 (launch.json command)
- WAIT several seconds for OpenOCD to halt target
- VScode now connects GDB to OpenOCD

- Can now set breakpoints (while CPU halted)
    and step through program via debug commands

- keep an eye on OpenOCD terminal-- will sometimes
    disconnect if commands are invalid or connect fails.
- Use xtensa-esp32-elf-gdb build/Alarm_Clock_v3.elf
    to launch GDB terminal through the idf.py if neccessary

## Troubleshooting

###Use GDB:
- From Debug Console, type
     -exec monitor reset halt


if ESP32 halts without debugger, try reflashing
- idf.py monitor
    check if panics are occuring

Clear memory completely:
idf.py fullclean
idf.py build
idf.py erase-flash
idf.py flash
idf.py monitor
