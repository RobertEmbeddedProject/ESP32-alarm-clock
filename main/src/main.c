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
#include "mp3.h"
#include "wifi.h"
#include <time.h>
#include "globals.h"
#include "nvs_flash.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"


//songs rotary encoder
gpio_config_t songs_pb;
pcnt_unit_handle_t pcnt_unit_songs = NULL;
int index_songs = 0;
int pulse_count_songs_prev = 0;
int pulse_count_songs_now = 0;
int array_songs[SONG_COUNT] = {};
int array_songs_max = sizeof(array_songs) / sizeof(array_songs[0]);

//alarm rotary encoder
gpio_config_t alarm_pb;
pcnt_unit_handle_t pcnt_unit_alarm = NULL;
int index_alarm = 0;
int pulse_count_alarm_prev = 0;
int pulse_count_alarm_now = 0;
int array_alarm[SONG_COUNT] = {};
int array_alarm_max = sizeof(array_alarm) / sizeof(array_alarm[0]);

//MP3 Player
bool music_playing = 0;
int song_playing = 999;


void display_task(void *arg){
    char index_text[32];
    char time_text[32];
    char alarm_text[32];

    while(1){

        //Time aquisition once per loop
        time_t now;
        struct tm current;
        time(&now);
        localtime_r(&now, &current);
        snprintf(time_text, sizeof(time_text),
                 "%02d:%02d",
                 current.tm_hour,
                 current.tm_min);

        snprintf(index_text, sizeof(index_text), "%d", index_songs);
        snprintf(alarm_text, sizeof(alarm_text), "%d", index_alarm);
        ssd1309_clear();
        
        ssd1309_draw_text(20, 3, "Time:");
        ssd1309_draw_text(70, 3, time_text);
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
        rotary_index(songs_pb, pcnt_unit_songs, &pulse_count_songs_prev, 
                        &pulse_count_songs_now, &index_songs, array_songs_max);
        rotary_index(alarm_pb, pcnt_unit_alarm, &pulse_count_alarm_prev, 
                        &pulse_count_alarm_now, &index_alarm, array_alarm_max);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void serial_monitor(void *args){
    while(1){
        //ESP_LOGI("test", "Button Value = %d", gpio_get_level(GPIO_NUM_39));
        //ESP_LOGI("test", "Button Value = %d", gpio_get_level(GPIO_NUM_36));

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void song_task(void *args){
    ESP_LOGI("SONG", "song_task started");
    while (1) {
        if (gpio_get_level(GPIO_NUM_39) == 0 && song_playing != index_songs) {
            mp3_cmd(CMD_PLAY_W_INDEX, index_songs+1);
            music_playing = 1;
            song_playing = index_songs;
            vTaskDelay(pdMS_TO_TICKS(500)); // prevent command spam
        }
       else if (gpio_get_level(GPIO_NUM_39) == 0 && music_playing == 1 && index_songs == song_playing){
            mp3_cmd(CMD_STOP, 0);
            music_playing = 0;
            song_playing  = 999;
            vTaskDelay(pdMS_TO_TICKS(500));
       }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void app_main(void)
{
    OLED_init();
    ssd1309_clear();
    ssd1309_display();

    rotary_init(ROTARY_KNOB_SONGS, &pcnt_unit_songs);
    rotary_init(ROTARY_KNOB_ALARM, &pcnt_unit_alarm);

    wifi_init();

    //Time Aquisition, delete after testing
    time_t now;
    struct tm current;
    time(&now);
    localtime_r(&now, &current);
    printf("%02d:%02d:%02d\n",
       current.tm_hour,
       current.tm_min,
       current.tm_sec);


    mp3_init();
    vTaskDelay(pdMS_TO_TICKS(500)); //delay must be 500<
    mp3_cmd(CMD_SET_VOLUME, 30);
    for(int i=0; i<SONG_COUNT; i++){
        array_songs[i] = i*100;
    }

    xTaskCreate(serial_monitor, "serial_monitor", 2048, NULL, 1, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 4, NULL);
    xTaskCreate(song_task, "song_task", 2048, NULL, 6, NULL);
    xTaskCreate(rotary_task, "rotary_task", 2048, NULL, 7, NULL);
    




    

}


