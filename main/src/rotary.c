/*
KY-040 Rotary Encoder User Inputs, 4x quadrature decoding.
A B
0 0
1 0
1 1
0 1
0 0
reference PCNT guide from ESP-IDF HAL
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/pcnt.html
*/

#include "rotary.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include <math.h>

#define PCNT_HI_LIM 1000
#define PCNT_LO_LIM -1000

#define ROTARY_SONGS_GPIO_A 32
#define ROTARY_SONGS_GPIO_B 33
#define ROTARY_SONGS_SW 39

#define ROTARY_ALARM_GPIO_A 25
#define ROTARY_ALARM_GPIO_B 26
#define ROTARY_ALARM_SW 36

/*
static bool pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t queue_rotary = (QueueHandle_t)user_ctx;
    // send event data to queue, from this interrupt callback
    xQueueSendFromISR(queue_rotary, &(edata->watch_point_value), &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}
*/

void rotary_init(rotary_knob_t rotary_knob, pcnt_unit_handle_t *pcnt_unit_out)
{

    gpio_num_t GPIO_A;
    gpio_num_t GPIO_B;
    gpio_num_t GPIO_SW;
    const char *tag = NULL;

    switch (rotary_knob)
    {
    case ROTARY_KNOB_SONGS:
        GPIO_A = ROTARY_SONGS_GPIO_A;
        GPIO_B = ROTARY_SONGS_GPIO_B;
        GPIO_SW = ROTARY_SONGS_SW;
        tag = "songs";
        break;
    case ROTARY_KNOB_ALARM:
        GPIO_A = ROTARY_ALARM_GPIO_A;
        GPIO_B = ROTARY_ALARM_GPIO_B;
        GPIO_SW = ROTARY_ALARM_SW;
        tag = "alarm";
        break;
    default:
        abort();
    }

    ESP_LOGI(tag, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HI_LIM,
        .low_limit = PCNT_LO_LIM,
    };
    pcnt_unit_handle_t pcnt_unit_in = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit_in));

    ESP_LOGI(tag, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_in, &filter_config));

    ESP_LOGI(tag, "install pcnt channels");

    //channel A
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = GPIO_A,
        .level_gpio_num = GPIO_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_in, &chan_a_config, &pcnt_chan_a));
    
    //channel B
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = GPIO_B,
        .level_gpio_num = GPIO_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_in, &chan_b_config, &pcnt_chan_b));

    //from HAL
    gpio_config_t rotary_pb = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPIO_SW),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&rotary_pb);


    ESP_LOGI(tag, "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_LOGI(tag, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_in));
    ESP_LOGI(tag, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_in));
    ESP_LOGI(tag, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_in));

    *pcnt_unit_out = pcnt_unit_in;
}

void rotary_index(pcnt_unit_handle_t pcnt_unit_out, int *pulse_prev,
                    int *pulse_now, int *index_out, int array_size)
{
    int pulse_raw = 0;

    *pulse_prev = *pulse_now;
    ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit_out, &pulse_raw));
    *pulse_now = pulse_raw;

    int delta = (*pulse_now / 4) - (*pulse_prev / 4);

    if (delta == 0) {
        return;
    }

    *index_out += delta;

    while (*index_out >= array_size) {
        *index_out -= array_size;
    }

    while (*index_out < 0) {
        *index_out += array_size;
    }

    if (abs(pulse_raw) > 1000) {
        ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_out));
        *pulse_prev = 0;
        *pulse_now = 0;
    }


}
    
