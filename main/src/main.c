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
#include "nvs_flash.h"
#include "graphics_bitmaps.h"


#define DISPLAY_DIM_TIMEOUT_MS    3000
#define DISPLAY_OFF_TIMEOUT_MS    5000

void display_splash(void);
//task notification, not a semaphore
static TaskHandle_t progress_bar_t = NULL;
static TaskHandle_t app_main_t = NULL;
TaskHandle_t display_task_t = NULL;

//RTOS Tasks
void splash_load_task(void *arg);
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
int index_alarm_hour = 6;
int index_alarm_minute = 30;
int pulse_count_alarm_prev = 0;
int pulse_count_alarm_now = 0;
int alarm_hour = 0;
int alarm_min = 0;

//alarm state
enum alarm{
  ALARM_IDLE,
  ALARM_CONFIG_HOUR,
  ALARM_CONFIG_MINUTE,
  ALARM_ARMED,
  ALARM_TRIGGERED,
  ALARM_SNOOZED
} alarm;
bool s_acked;

enum brightness brightness;
enum display_screen display_screen;

//OLED Display Formatting
int display_alarm_hour;
char *alarm_ampm;
int display_clock_hour;
char *clock_ampm;
float display_sleep_hours;

//MP3 Player
bool music_playing = 0;
int song_playing = 999;


void app_main(void)
{
    app_main_t = xTaskGetCurrentTaskHandle();

    OLED_init();
    ssd1309_clear();
    ssd1309_display();

    display_splash();
    xTaskCreate(splash_load_task, "splash load task", 2048, NULL, 5, &progress_bar_t);

    wifi_init();
    xTaskNotifyGive(progress_bar_t);

    alarm_min = index_alarm_minute;
    alarm_hour = index_alarm_hour;

    rotary_init(ROTARY_KNOB_SONGS, &pcnt_unit_songs);
    rotary_init(ROTARY_KNOB_ALARM, &pcnt_unit_alarm);

    mp3_init();
    vTaskDelay(pdMS_TO_TICKS(500)); //delay must be 500<
    mp3_cmd(CMD_SET_VOLUME, 30);
    for(int i=0; i<SONG_COUNT; i++){
        array_songs[i] = i*100;
    }

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // wait until splash task is done
    xTaskCreate(display_task, "display_task", 4096, NULL, 4, NULL);
    xTaskCreate(alarm_task, "alarm_task", 2048, NULL, 5, NULL);
    xTaskCreate(song_task, "song_task", 2048, NULL, 6, NULL);
    xTaskCreate(rotary_task, "rotary_task", 2048, NULL, 7, &display_task_t);

}

void display_splash(void){
    char wifi_text[32];
    char blank[32];

    display_screen = SCREEN_SPLASH;
    ssd1309_clear();
    update_display_info(wifi_text, blank, blank, blank, blank, blank);
    
    ssd1309_draw_text(64, 0, "WiFi:");
    ssd1309_draw_text(49, 1, "Connecting");
    ssd1309_draw_text(48, 6, "Loading");
    ssd1309_draw_xbm(0, 0, 49, 49, image_big_clock);
    ssd1309_draw_xbm(1, 57, 126, 8, image_progressbar);
    ssd1309_display();
}

void splash_load_task(void *arg){ 
        progress_bar_fill(0,8,200,800);
        progress_bar_fill(8,15,20,1200);
        progress_bar_fill(15,24,2,1500);
        progress_bar_fill(24,27,30,800);
        progress_bar_fill(27,45,5,400);
        progress_bar_fill(45,54,20,1);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        progress_bar_fill(54,63,6,1);
        xTaskNotifyGive(app_main_t); 
    vTaskDelete(NULL);
}

void display_task(void *arg){

    char wifi_text[32];
    char time_text[32];
    char alarm_hour_text[32];
    char alarm_minute_text[32];
    char sleep_text[32];
    char index_text[32];

    TickType_t last_activity_ticks = xTaskGetTickCount();

    while(1){

        if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            last_activity_ticks = xTaskGetTickCount();
        }
        
        update_display_info(wifi_text, time_text, alarm_hour_text, alarm_minute_text, sleep_text, index_text);
        ssd1309_clear();
        
        ssd1309_draw_text(64, 0, "WiFi: ");
        ssd1309_draw_text(104, 0, wifi_text);
        ssd1309_draw_text(20, 2, "Time:");
        ssd1309_draw_text(62, 2, time_text);
        ssd1309_draw_text(12, 3, "Alarm: ");
        ssd1309_draw_text(78, 3, ":"); 

        //invisible for 30ms, visible for 100ms, repeat every (total) 130ms
        bool s_blink_hour = (alarm == ALARM_CONFIG_HOUR) && ((xTaskGetTickCount() % pdMS_TO_TICKS(100)) < pdMS_TO_TICKS(30));
        if(!s_blink_hour){
    
            ssd1309_draw_text(62, 3, alarm_hour_text);
        }
        bool s_blink_minute = (alarm == ALARM_CONFIG_MINUTE) && ((xTaskGetTickCount() % pdMS_TO_TICKS(600)) < pdMS_TO_TICKS(200));
        if(!s_blink_minute){
            ssd1309_draw_text(86, 3, alarm_minute_text);
        }
        if(alarm == ALARM_ARMED){
            ssd1309_draw_xbm(0, 1, 16, 16, armed_clock_bmp);
            ssd1309_draw_text(15, 0, "Armed");
            if(0.0f < display_sleep_hours && display_sleep_hours <= 1.0f){ssd1309_draw_text(12, 4, "Sleep:    hour");}
            else{ssd1309_draw_text(12, 4, "Sleep:     hrs");}
            ssd1309_draw_text(62, 4, sleep_text);
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
        //Songs Rotary
        int previous_song_index = index_songs;
        rotary_index(pcnt_unit_songs, &pulse_count_songs_prev, 
                        &pulse_count_songs_now, &index_songs, array_songs_max);
        if (index_songs != previous_song_index) {
            screen_activity();
        }

        //Alarm Rotary
        int previous_alarm_index_hour = index_alarm_hour;
        int previous_alarm_index_minute = index_alarm_minute;
        if(alarm == ALARM_CONFIG_HOUR){
            rotary_index(pcnt_unit_alarm, &pulse_count_alarm_prev, 
                            &pulse_count_alarm_now, &index_alarm_hour, 24);
            }             
        else if(alarm == ALARM_CONFIG_MINUTE){
        rotary_index(pcnt_unit_alarm, &pulse_count_alarm_prev, 
                        &pulse_count_alarm_now, &index_alarm_minute, 60); 
        }                               
        else{ //only start counting when in config mode
            ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_alarm));
            pulse_count_alarm_prev = 0;
            pulse_count_alarm_now = 0;
        }
        if (index_alarm_hour != previous_alarm_index_hour ||
                index_alarm_minute != previous_alarm_index_minute) {
                screen_activity();
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void song_task(void *args){
    ESP_LOGI("SONG", "song_task started");
    while (1) {
        if (gpio_get_level(GPIO_NUM_39) == 0 && song_playing != index_songs) {
            mp3_cmd(CMD_PLAY_W_INDEX, index_songs+1);
            music_playing = 1;
            song_playing = index_songs;
            screen_activity();
            vTaskDelay(pdMS_TO_TICKS(500));
        }
       else if (gpio_get_level(GPIO_NUM_39) == 0 && music_playing == 1 && index_songs == song_playing){
            mp3_cmd(CMD_STOP, 0);
            music_playing = 0;
            song_playing  = 999;
            screen_activity();
            vTaskDelay(pdMS_TO_TICKS(500));
       }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void alarm_task(void *args){
    ESP_LOGI("ALARM", "alarm_task started");
    time_t now;
    struct tm current;
    TickType_t last_time_check = 0;
    while (1) {
        //only check time once per second
        if ((xTaskGetTickCount() - last_time_check) >= pdMS_TO_TICKS(1000)) {
            last_time_check = xTaskGetTickCount();
            time(&now);
            localtime_r(&now, &current);
        }

        if(gpio_get_level(GPIO_NUM_36) == 0){
            //screen_activity();
            switch (alarm){
                case ALARM_IDLE:
                    alarm = ALARM_CONFIG_HOUR;
                    pcnt_unit_clear_count(pcnt_unit_alarm);
                    pulse_count_alarm_prev = 0;
                    pulse_count_alarm_now  = 0;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;
                case ALARM_CONFIG_HOUR:
                    alarm = ALARM_CONFIG_MINUTE;
                    pcnt_unit_clear_count(pcnt_unit_alarm);
                    pulse_count_alarm_prev = 0;
                    pulse_count_alarm_now  = 0;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;
                case ALARM_CONFIG_MINUTE:
                    alarm = ALARM_ARMED;
                    s_acked = 0;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;
                case ALARM_ARMED:
                    alarm = ALARM_IDLE;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;
                case ALARM_TRIGGERED:
                    alarm = ALARM_IDLE;
                    s_acked = 1;
                    alarm_min--; //prevent immediate re-trigger
                    mp3_cmd(CMD_STOP, 0);
                    music_playing = 0;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;
                default:
            }
        }

        if(alarm == ALARM_ARMED && s_acked == 0 &&
                    current.tm_hour == alarm_hour && current.tm_min == alarm_min){
                alarm = ALARM_TRIGGERED;
                mp3_cmd(CMD_PLAY_W_INDEX, index_songs+1);
                music_playing = 1;
            }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

