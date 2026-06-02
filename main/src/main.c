
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
#include "globals.h"

#define testdelay 500 // milliseconds

pcnt_unit_handle_t pcnt_unit_songs = NULL;
int index_songs = 0;
int pulse_count_songs_prev = 0;
int pulse_count_songs_now = 0;
int array_songs[11] = {};
int array_songs_max = sizeof(array_songs) / sizeof(array_songs[0]);


void display_task(void *arg){
    char index_text[32];

    while(1){
        snprintf(index_text, sizeof(index_text), "%d", index_songs);
        ssd1309_clear();
        ssd1309_draw_text(20, 3, "index:");
        ssd1309_draw_text(70, 3, index_text);
        ssd1309_display();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void rotary_task(void *arg){
    while(1){
        rotary_index(pcnt_unit_songs, &pulse_count_songs_prev, &pulse_count_songs_now, array_songs, &index_songs, array_songs_max);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}


void app_main(void)
{
    
    OLED_init();
    ssd1309_clear();
    ssd1309_display();

    rotary_init(ROTARY_KNOB_SONGS, &pcnt_unit_songs);

    for(int i=0; i<11; i++){
        array_songs[i] = i*100;
    }

    xTaskCreate(display_task, "display_task", 8192, NULL, 8, NULL);
    xTaskCreate(rotary_task, "rotary_task", 4096, NULL, 9, NULL);
}

/*SSD1309 Test:
        vTaskDelay(pdMS_TO_TICKS(testdelay/2));
        ssd1309_clear();
        ssd1309_display();
        vTaskDelay(pdMS_TO_TICKS(testdelay));
        ssd1309_draw_text(20, 2, "test");
        ssd1309_display();
        */


