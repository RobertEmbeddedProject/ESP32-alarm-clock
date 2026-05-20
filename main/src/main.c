
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h" //RTOS implement for time vTaskDelay commands
#include "freertos/task.h"     //Task management API xTaskCreate(), vTaskDelete(), vTaskDelay()
#include "driver/gpio.h"
#include "main.h"
#include "ssd1309.h"
#include "rotary.h"

#define testdelay 500 // milliseconds

void app_main(void)
{
    pcnt_unit_handle_t pcnt_unit_songs = NULL;
    const char *songs_knob = "songs";
    int pulse_count_raw_songs = 0;
    int pulse_count_prev_songs = 0;
    int pulse_count_now_songs = 0;
    int index_songs = 0;
    int array_songs[64] = {};
    int array_songs_min = 0;
    int array_songs_max = 63; //use sizeof()
    for(int i=0; i<64; i++){
        array_songs[i] = i*100;
    }

    pcnt_unit_handle_t pcnt_unit_alarm = NULL;
    const char *alarm_knob = "alarm";
    int pulse_count_raw_alarm = 0;

    OLED_init();
    rotary_init(ROTARY_KNOB_SONGS, &pcnt_unit_songs);
    rotary_init(ROTARY_KNOB_ALARM, &pcnt_unit_alarm);

    while (1)
    {

        pulse_count_prev_songs = pulse_count_now_songs;
        // gets PCNT count by simple polling:
        ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit_songs, &pulse_count_raw_songs));
        ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit_alarm, &pulse_count_raw_alarm));
        pulse_count_now_songs = pulse_count_raw_songs;
        rotary_index(pulse_count_prev_songs, pulse_count_now_songs, &index_songs);
        if(abs(pulse_count_raw_songs)>100){
            pulse_count_raw_songs = 0;
            pulse_count_prev_songs = 0;
            pulse_count_now_songs = 0;
        }





        ESP_LOGI(songs_knob, "Pulse count: %d index: %d array[]: %d", 
                        pulse_count_raw_songs, index_songs, array_songs[index_songs]);
        vTaskDelay(pdMS_TO_TICKS(300));
        // ESP_LOGI(alarm_knob, "Pulse count: %d", pulse_count_alarm);
        // vTaskDelay(pdMS_TO_TICKS(300));
    }
}

/*SSD1309 Test:
        vTaskDelay(pdMS_TO_TICKS(testdelay/2));
        ssd1309_clear();
        ssd1309_display();
        vTaskDelay(pdMS_TO_TICKS(testdelay));
        ssd1309_draw_text(20, 2, "test");
        ssd1309_display();
        */
