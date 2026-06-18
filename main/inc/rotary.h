#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

// enumerate for GPIO cases
typedef enum
{
    ROTARY_KNOB_SONGS,
    ROTARY_KNOB_ALARM,
} rotary_knob_t;

void rotary_init(rotary_knob_t rotary_knob, pcnt_unit_handle_t *pcnt_unit_out);
void rotary_index_rollover(pcnt_unit_handle_t pcnt_unit_out, int *pulse_prev,
                    int *pulse_now, volatile int *index_out, int array_size);
void rotary_index_clamp(pcnt_unit_handle_t pcnt_unit_out, int *pulse_prev,
                    int *pulse_now, volatile int *index_out, int array_size,
                    int min_val, int max_val);
