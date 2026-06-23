#pragma once
#include <stdint.h>
#include "display_ssd1309.h"
#include "master_alarm.h"

//Display wake commands
#define WAKE_PARTIAL  1U
#define WAKE_FULL     2U

void display_task(void *arg);
void format_AM_PM(int input_hour, int *display_hour, char **ampm);
void update_display_info(char *wifi_text, char *time_text, char *time_ampm_text, char *alarm_hour_text,
                            char *alarm_minute_text, char *alarm_ampm_text, char *sleep_text,
                            char *index_text, int index_songs);
void screen_saver(enum alarm state, uint32_t *wake_type);
void screen_wake(uint32_t wake_type);
enum brightness get_brightness(void);
void progress_bar_fill(int start, int end, int lag_ms, int post_delay_ms);
void display_splash(void);
void splash_load_task(void *arg);
