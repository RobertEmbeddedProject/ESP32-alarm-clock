#include "master_alarm.h"
#include "display_application.h"
#include "mp3.h"
#include "rotary.h"
#include <time.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" 
#include <esp_log.h>
#include "driver/pulse_cnt.h"

//Master Alarm
static volatile enum alarm alarm_state = ALARM_IDLE;
static volatile int alarm_hour;
static volatile int alarm_min;
static bool s_acked = 1; //Actual alarm acknowledgement; disable and go back to idle

static volatile int whitenoise_selection = 0;
static volatile bool whitenoise_option_show = false;

extern TaskHandle_t song_playback_t;


void alarm_task(void *args){
    ESP_LOGI("ALARM", "alarm_task started");

    time_t now;
    struct tm current;
    TickType_t last_time_check = 0;

    //savestate for whitenoise selection return
    volatile int index_songs_snapshot = 0;

    while (1) {
        int alarm_hour = alarm_hour_get_index();
        int alarm_min  = alarm_minute_get_index();
        int index_songs = songs_get_index();

        //only check time once per second
        if ((xTaskGetTickCount() - last_time_check) >= pdMS_TO_TICKS(1000)) {
            last_time_check = xTaskGetTickCount();
            time(&now);
            localtime_r(&now, &current);
        }

        if(alarm_state != ALARM_ARMED){
            whitenoise_selection = 0;
        }

        //Rotary pushbutton progresses the alarm states
        if(gpio_get_level(GPIO_NUM_36) == 0){
            screen_wake(WAKE_FULL);
            switch (alarm_state){
                case ALARM_IDLE:
                    alarm_state = ALARM_CONFIG_HOUR;
                    clear_pulse_count_alarm();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case ALARM_CONFIG_HOUR:
                    alarm_state = ALARM_CONFIG_MINUTE;
                    clear_pulse_count_alarm();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case ALARM_CONFIG_MINUTE:
                    alarm_state = ALARM_CONFIG_WHITENOISE;
                    index_songs_snapshot = index_songs;
                    whitenoise_option_show = true;
                    songs_set_index(69); //set up white noise selection for next state
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case ALARM_CONFIG_WHITENOISE:
                    alarm_state = ALARM_ARMED;
                    clear_pulse_count_songs();
                    if(whitenoise_option_show == 0){
                        whitenoise_selection = index_songs;
                        mp3_request(MP3_REQ_PLAY_INDEX, whitenoise_selection+1);
                    }  
                    songs_set_index(index_songs_snapshot);
                    s_acked = 0;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case ALARM_ARMED:
                    alarm_state = ALARM_IDLE; 
                    s_acked = 1;
                    //only stop music if white noise was playing, not casual listening
                    if(whitenoise_selection !=0 ){
                        mp3_request(MP3_REQ_STOP, 0);
                    }
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case ALARM_TRIGGERED:
                    alarm_state = ALARM_IDLE;
                    s_acked = 1;
                    mp3_request(MP3_REQ_STOP, 0);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case ALARM_SNOOZED:
                    alarm_state = ALARM_IDLE;
                    mp3_request(MP3_REQ_STOP, 0);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                default:
            }
        }
        //Alarm trigger
        if(((alarm_state == ALARM_ARMED) 
                && (current.tm_hour == alarm_hour && current.tm_min == alarm_min)) 
                && (s_acked == 0))     // not acked by user 
                    {
                        xTaskNotifyGive(song_playback_t); 
                    }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

enum alarm alarm_get_state(void)
{
    return alarm_state;
}

void alarm_set_state(enum alarm state)
{
    alarm_state = state;
}

bool check_whitenoise_config(void){
    return whitenoise_option_show;
}

void set_whitenoise_config(bool state){
    whitenoise_option_show = state;
}
