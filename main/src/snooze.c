#include "snooze.h"
#include "master_alarm.h"
#include "radar_snooze.h"
#include "display_application.h"
#include "rotary.h"
#include "mp3.h"
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>

#define SNOOZETIMER_MS    5000 //snooze time until re-trigger
#define RADAR_HOLD_MS     300
#define RADAR_COOLDOWN_MS 3000

static bool snooze_radar_detected = false;
static TickType_t snooze_timer;
static TickType_t snooze_time_display;

void snooze_task(void *args){
    ESP_LOGI("SNOOZE", "snooze_task started");

    //Radar Polling 
    int still_counts = 0;
    TickType_t radar_last_trigger = 0;
    TickType_t last_time_check = 0;
    bool snooze_timeout = true;

    while (1) {

        enum alarm alarm_state = alarm_get_state();
        enum brightness brightness = get_brightness();
        int index_songs = songs_get_index();
        

        
        //Radar Polling
        TickType_t now_ticks = xTaskGetTickCount();
        still_counts = radar_get_stationary() ? still_counts + 1 : 0;
        if (still_counts >= (RADAR_HOLD_MS / 20) &&
            (now_ticks - radar_last_trigger) >= pdMS_TO_TICKS(RADAR_COOLDOWN_MS)) {
            
            snooze_radar_detected = true;
            radar_last_trigger = now_ticks;
            still_counts = 0;
        }
        
        //Snooze feature
        if (snooze_radar_detected) {  
            if(alarm_state == ALARM_TRIGGERED){
                alarm_set_state(ALARM_SNOOZED);
                screen_wake(WAKE_FULL);
                mp3_request(MP3_REQ_SNOOZE_STOP, 0);
                snooze_timeout = false;
                last_time_check = xTaskGetTickCount();
            }
            else if(brightness == DISPLAY_OFF){
                screen_wake(WAKE_PARTIAL);
            }

            snooze_radar_detected = false;

            vTaskDelay(pdMS_TO_TICKS(500));
        }

        //Alarm Re-trigger after Snooze timeout
        snooze_timer = (xTaskGetTickCount() - last_time_check);

        //displayed countdown time in minutes
        snooze_time_display = pdTICKS_TO_MS((pdMS_TO_TICKS(SNOOZETIMER_MS) - snooze_timer))/1000/60;

        snooze_timeout = (snooze_timer >= pdMS_TO_TICKS(SNOOZETIMER_MS));
        if(alarm_state == ALARM_SNOOZED && snooze_timeout == true){
            alarm_set_state(ALARM_TRIGGERED);
            screen_wake(WAKE_FULL);

            mp3_request(MP3_REQ_START_ALARM, index_songs + 1);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

int get_elapsed_snooze_time(void){
    return snooze_timer;
}
