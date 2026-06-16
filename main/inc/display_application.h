#pragma once
#include <stdint.h>
#include "display_ssd1309.h"

//Display wake commands
#define WAKE_PARTIAL  1U
#define WAKE_FULL     2U

void format_AM_PM(int input_hour, int *display_hour, char **ampm);
void update_display_info(char *wifi_text, char *time_text, char *alarm_hour_text,
            char *alarm_minute_text, char *alarm_ampm_text, char *sleep_text, char *index_text);
void screen_saver(enum alarm state, uint32_t *wake_type);
void screen_wake(uint32_t wake_type);
