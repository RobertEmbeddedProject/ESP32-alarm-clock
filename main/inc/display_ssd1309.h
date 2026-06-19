#pragma once

#include <stdint.h>

//#include "driver/i2c.h" use new v5.5 header per manual
#include "driver/i2c_master.h"

//OLED IC Requirements
//Each buffer is 8 pixels vertically from bottom to top in order to create a "page"
#define OLED_WIDTH                  128 //pixels
#define OLED_HEIGHT                 64  //pixels
#define OLED_BUF_SIZE               (OLED_WIDTH * OLED_HEIGHT / 8) //1024 total pages
extern uint8_t oled_buffer[OLED_BUF_SIZE];

//OLED Display Brightness Mode
enum brightness{
  DISPLAY_OFF,
  DISPLAY_DIM,
  DISPLAY_BRIGHT
};

void I2C_init(void);
void OLED_init(void);
void OLED_cmd(uint8_t cmd);

void ssd1309_clear(void);
void ssd1309_fill(void);
void ssd1309_display(void);
void cmd_display_mode(enum brightness state);



