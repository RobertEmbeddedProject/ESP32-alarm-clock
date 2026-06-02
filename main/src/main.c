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
#include "songs.h"
#include "globals.h"

#define testdelay 500 // milliseconds

pcnt_unit_handle_t pcnt_unit_songs = NULL;
int index_songs = 0;
int pulse_count_songs_prev = 0;
int pulse_count_songs_now = 0;
int array_songs[SONG_COUNT] = {};
int array_songs_max = sizeof(array_songs) / sizeof(array_songs[0]);


pcnt_unit_handle_t pcnt_unit_alarm = NULL;
int index_alarm = 0;
int pulse_count_alarm_prev = 0;
int pulse_count_alarm_now = 0;
int array_alarm[SONG_COUNT] = {};
int array_alarm_max = sizeof(array_alarm) / sizeof(array_alarm[0]);



const char *test = NULL;

void display_task(void *arg){
    char index_text[32];
    char alarm_text[32];

    while(1){
        snprintf(index_text, sizeof(index_text), "%d", index_songs);
        snprintf(alarm_text, sizeof(alarm_text), "%d", index_alarm);
        ssd1309_clear();
        ESP_LOGI("test", "songs max = %d", array_songs_max);
        ssd1309_draw_text(20, 4, "Alarm:");
        ssd1309_draw_text(70, 4, alarm_text);
        ssd1309_draw_text(0, 6, "Song Index:");
        ssd1309_draw_text(90, 6, index_text);
        ssd1309_draw_text(0, 7, songlist[index_songs]);
        ssd1309_display();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void rotary_task(void *arg){
    while(1){
        rotary_index(pcnt_unit_songs, &pulse_count_songs_prev, 
                        &pulse_count_songs_now, &index_songs, array_songs_max);
        rotary_index(pcnt_unit_alarm, &pulse_count_alarm_prev, 
                        &pulse_count_alarm_now, &index_alarm, array_alarm_max);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}


void app_main(void)
{
    
    OLED_init();
    ssd1309_clear();
    ssd1309_display();

    rotary_init(ROTARY_KNOB_SONGS, &pcnt_unit_songs);
    rotary_init(ROTARY_KNOB_ALARM, &pcnt_unit_alarm);

    for(int i=0; i<SONG_COUNT; i++){
        array_songs[i] = i*100;
    }

    xTaskCreate(display_task, "display_task", 8192, NULL, 4, NULL);
    xTaskCreate(rotary_task, "rotary_task", 4096, NULL, 7, NULL);
    
}

/*SSD1309 Test:
        vTaskDelay(pdMS_TO_TICKS(testdelay/2));
        ssd1309_clear();
        ssd1309_display();
        vTaskDelay(pdMS_TO_TICKS(testdelay));
        ssd1309_draw_text(20, 2, "test");
        ssd1309_display();
        */


