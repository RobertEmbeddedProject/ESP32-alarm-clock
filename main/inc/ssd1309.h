
#include <string.h>
#include <time.h>

//#include "driver/i2c.h" use new v5.5 header per manual
#include "driver/i2c_master.h"

void I2C_init(void);
void OLED_init(void);
void OLED_cmd(uint8_t cmd);

void ssd1309_clear(void);
void ssd1309_fill(void);
void ssd1309_display(void);

void ssd1309_draw_xbm(int x, int y, int w, int h, const uint8_t *bitmap);
void ssd1309_draw_pixel(int x, int y, bool on);
void ssd1309_draw_char(int x, int y, char c);
void ssd1309_draw_text(int x, int y, const char *text);
void format_AM_PM(int input_hour, int *display_hour, char **ampm);
void update_display_info(char *time_text, char *alarm_text, char *index_text);
