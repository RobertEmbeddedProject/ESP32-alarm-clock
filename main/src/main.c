
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"   //RTOS implement for time vTaskDelay commands
#include "freertos/task.h"       //Task management API xTaskCreate(), vTaskDelete(), vTaskDelay()
#include "driver/gpio.h"
#include "main.h"

#include "ssd1309.h"
#include "rotary.h"





#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_sleep.h"


#define testdelay 500 //milliseconds


void app_main(void)
{


    const char *songs_knob = "songs";
    const char *alarm_knob = "alarm";

    pcnt_unit_handle_t pcnt_unit_songs = NULL;
    QueueHandle_t queue_rotary_songs = NULL;

    pcnt_unit_handle_t pcnt_unit_alarm = NULL;
    QueueHandle_t queue_rotary_alarm = NULL;

    int pulse_count_songs = 0;
    int event_count_songs = 0;
    int pulse_count_alarm = 0;
    int event_count_alarm = 0;

    //OLED_init();

    rotary_init(ROTARY_KNOB_SONGS, &pcnt_unit_songs, &queue_rotary_songs);
    rotary_init(ROTARY_KNOB_ALARM, &pcnt_unit_alarm,&queue_rotary_alarm);

    while (1) {
        /*
        vTaskDelay(pdMS_TO_TICKS(testdelay/2));
        ssd1309_clear();
        ssd1309_display();
        vTaskDelay(pdMS_TO_TICKS(testdelay));
        ssd1309_draw_text(20, 2, "test");
        ssd1309_display();
        */

        if (xQueueReceive(queue_rotary_songs, &event_count_songs, pdMS_TO_TICKS(1000))) {
            ESP_LOGI(songs_knob, "Watch point event, count: %d", event_count_songs);
        } else {
            ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit_songs, &pulse_count_songs));
            ESP_LOGI(songs_knob, "Pulse count: %d", pulse_count_songs);
        }
        
        if (xQueueReceive(queue_rotary_alarm, &event_count_alarm, pdMS_TO_TICKS(1000))) {
            ESP_LOGI(alarm_knob, "Watch point event, count: %d", event_count_alarm);
        } else {
            ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit_alarm, &pulse_count_alarm));
            ESP_LOGI(alarm_knob, "Pulse count: %d", pulse_count_alarm);
        }

    }
}



