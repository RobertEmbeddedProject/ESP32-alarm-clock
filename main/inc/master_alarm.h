#pragma once
#include <stdbool.h>

enum alarm{
  ALARM_IDLE,
  ALARM_CONFIG_HOUR,
  ALARM_CONFIG_MINUTE,
  ALARM_CONFIG_WHITENOISE,
  ALARM_ARMED,
  ALARM_TRIGGERED,
  ALARM_SNOOZED,
  ALARM_ERROR
};

void alarm_task(void *arg);
enum alarm alarm_get_state(void);
void alarm_set_state(enum alarm state);
bool check_whitenoise_config(void);
void set_whitenoise_config(bool state);
void alarm_restore_armed(void);
