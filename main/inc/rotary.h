#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include <stdbool.h>

// enumerate for GPIO cases
typedef enum
{
    ROTARY_KNOB_SONGS,
    ROTARY_KNOB_ALARM,
} rotary_knob_t;

void rotary_task(void *arg);
void rotary_init(rotary_knob_t rotary_knob, pcnt_unit_handle_t *pcnt_unit_out);

void rotary_init_songs_and_alarm(void);

void rotary_index_rollover(pcnt_unit_handle_t pcnt_unit_out, int *pulse_prev,
                    int *pulse_now, volatile int *index_out, int array_size);
void rotary_index_clamp(pcnt_unit_handle_t pcnt_unit_out, int *pulse_prev,
                    int *pulse_now, volatile int *index_out, int array_size,
                    int min_val, int max_val);

//API
int songs_get_index(void);
void songs_set_index(int index);
int alarm_hour_get_index(void);
void alarm_hour_set_index(int index);
int alarm_minute_get_index(void);
void alarm_minute_set_index(int index);
void clear_pulse_count_songs(void);
void clear_pulse_count_alarm(void);
