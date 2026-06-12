#include <stdint.h>
#include <stdbool.h>
//#include <stdlib.h>
//#include <math.h>
#include <string.h>
#include <esp_log.h>
#include "esp_err.h"
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
#include "graphics_bitmaps.h"

void display_splash(void);
void display_task(void *arg);
void rotary_task(void *arg);
void alarm_task(void *arg);
void song_task(void *arg);

//songs rotary encoder
pcnt_unit_handle_t pcnt_unit_songs = NULL;
int index_songs = 0;
int pulse_count_songs_prev = 0;
int pulse_count_songs_now = 0;
int array_songs[SONG_COUNT] = {};
int array_songs_max = sizeof(array_songs) / sizeof(array_songs[0]);

//alarm rotary encoder
pcnt_unit_handle_t pcnt_unit_alarm = NULL;
int index_alarm = 0;
int pulse_count_alarm_prev = 0;
int pulse_count_alarm_now = 0;
int alarm_hour = 0;
int alarm_min = 0;

//alarm state
enum master_alarm{
  s_disarmed,
  s_config,
  s_armed
} volatile alarm;

//OLED Display Formatting
int display_alarm_hour;
char *alarm_ampm;
int display_clock_hour;
char *clock_ampm;

//MP3 Player
bool music_playing = 0;
int song_playing = 999;


void app_main(void)
{
    OLED_init();
    ssd1309_clear();
    ssd1309_display();

    display_splash();

    wifi_init();

    rotary_init(ROTARY_KNOB_SONGS, &pcnt_unit_songs);
    rotary_init(ROTARY_KNOB_ALARM, &pcnt_unit_alarm);

    mp3_init();
    vTaskDelay(pdMS_TO_TICKS(500)); //delay must be 500<
    mp3_cmd(CMD_SET_VOLUME, 30);
    for(int i=0; i<SONG_COUNT; i++){
        array_songs[i] = i*100;
    }

    xTaskCreate(display_task, "display_task", 4096, NULL, 4, NULL);
    xTaskCreate(alarm_task, "alarm_task", 2048, NULL, 5, NULL);
    xTaskCreate(song_task, "song_task", 2048, NULL, 6, NULL);
    xTaskCreate(rotary_task, "rotary_task", 2048, NULL, 7, NULL);

}

void display_splash(void){
    char wifi_text[32];
    char blank[32];

    ssd1309_clear();
    update_display_info(wifi_text, blank, blank, blank);
    
    ssd1309_draw_text(56, 0, "WiFi:");
    ssd1309_draw_text(104, 0, wifi_text);
    ssd1309_draw_text(8, 8, "Initializing...");
    ssd1309_draw_xbm(0, 0, 57, 57, big_clock);

    ssd1309_display();
}

void display_task(void *arg){

    char wifi_text[32];
    char time_text[32];
    char alarm_text[32];
    char index_text[32];

    while(1){

        update_display_info(wifi_text, time_text, alarm_text, index_text);

        ssd1309_clear();
        
        ssd1309_draw_text(56, 0, "WiFi: ");
        ssd1309_draw_text(104, 0, wifi_text);
        ssd1309_draw_text(12, 3, "Time:");
        ssd1309_draw_text(54, 3, time_text);

        bool s_blink = (alarm == s_config && ((xTaskGetTickCount() / pdMS_TO_TICKS(250)) % 2) == 0);
        if(!s_blink){
            ssd1309_draw_text(4, 4, "Alarm: ");
            ssd1309_draw_text(54, 4, alarm_text);
        }
        if(alarm == s_armed){
            ssd1309_draw_xbm(0, 1, 16, 16, armed_clock_bmp);
            ssd1309_draw_text(15, 1, "Armed");
        }

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
        if(alarm == s_config){
            rotary_index(pcnt_unit_alarm, &pulse_count_alarm_prev, 
                            &pulse_count_alarm_now, &index_alarm, 288);
            }                                           // 288*5/60 = 24 hours
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}


void song_task(void *args){
    ESP_LOGI("SONG", "song_task started");
    while (1) {
        if (gpio_get_level(GPIO_NUM_39) == 0 && song_playing != index_songs) {
            mp3_cmd(CMD_PLAY_W_INDEX, index_songs+1);
            music_playing = 1;
            song_playing = index_songs;
            vTaskDelay(pdMS_TO_TICKS(500));
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

void alarm_task(void *args){
    ESP_LOGI("ALARM", "alarm_task started");
    while (1) {
        if(gpio_get_level(GPIO_NUM_36) == 0){
            switch (alarm){
                case s_disarmed:
                    alarm = s_config;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;
                case s_config:
                    alarm = s_armed;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;
                case s_armed:
                    alarm = s_disarmed;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;
                    
                default:
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

