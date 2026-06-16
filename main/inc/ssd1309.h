
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"

//#include "driver/i2c.h" use new v5.5 header per manual
#include "driver/i2c_master.h"

//Master Alarm States
enum alarm{
  ALARM_IDLE,
  ALARM_CONFIG_HOUR,
  ALARM_CONFIG_MINUTE,
  ALARM_ARMED,
  ALARM_TRIGGERED,
  ALARM_SNOOZED
} extern volatile alarm_state;

//OLED Display Brightness Mode
enum brightness{
  DISPLAY_OFF,
  DISPLAY_DIM,
  DISPLAY_BRIGHT
};

//OLED Display Screen Selection
enum display_screen{
  SCREEN_SPLASH,
  SCREEN_MAIN,
  SCREEN_IDLE
};

void I2C_init(void);
void OLED_init(void);
void OLED_cmd(uint8_t cmd);

void ssd1309_clear(void);
void ssd1309_fill(void);
void ssd1309_display(void);

void ssd1309_draw_xbm(int x, int y, int w, int h, const uint8_t *bitmap);
void progress_bar_fill(int start, int end, int lag_ms, int post_delay_ms);
void ssd1309_draw_pixel(int x, int y, bool on);
void ssd1309_draw_char(int x, int y, char c);
void ssd1309_draw_text(int x, int y, const char *text);
void format_AM_PM(int input_hour, int *display_hour, char **ampm);
void update_display_info(char *wifi_text, char *time_text, char *alarm_hour_text,
            char *alarm_minute_text, char *alarm_ampm_text, char *sleep_text, char *index_text);
void cmd_display_mode(enum brightness state);
void screen_saver(enum alarm *state);
void screen_activity(void);

