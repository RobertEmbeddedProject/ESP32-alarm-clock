
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
GPIO 1  = Radar Snooze Consumed Data (UART)
GPIO 4  = MP3 Module Power (Transistor Circuit)
GPIO 17 = MP3 Module Produced Data   (UART)
GPIO 21 = SSD1309 SDA (I2C)
GPIO 22 = SSD1309 SCL (I2C)
GPIO 23 = SSD1309 RESET
GPIO 36 = Alarm SW (vp)
GPIO 25 = Alarm CLK
GPIO 26 = Alarm DATA
GPIO 32 = Songs CLK
GPIO 33 = Songs DATA
GPIO 39 = Songs SW (vn)



