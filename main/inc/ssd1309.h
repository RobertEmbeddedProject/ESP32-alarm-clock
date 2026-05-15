
#include <string.h>

//#include "driver/i2c.h" use new v5.5 header per manual
#include "driver/i2c_master.h"

void I2C_init(void);
void OLED_init(void);
void OLED_cmd(uint8_t cmd);

void ssd1309_draw_hline(int x_start, int x_end, int y);
void ssd1309_clear(void);
void ssd1309_display(void);