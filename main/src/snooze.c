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

#define SNOOZETIMER_MS 12000 //snooze time until re-trigger
#define RADAR_HOLD_MS      300
#define RADAR_COOLDOWN_MS 3000

static bool snooze_radar_detected = false;
extern TickType_t radar_last_trigger;
extern TickType_t last_time_check;

void snooze_task(void *args){
    ESP_LOGI("SNOOZE", "snooze_task started");

    enum brightness brightness = get_brightness();

    int index_songs = songs_get_index();

    //Radar Polling 
    int still_counts = 0;
    TickType_t radar_last_trigger = 0;
    TickType_t last_time_check = 0;
    bool snooze_timeout = true;

    while (1) {

        enum alarm alarm_state = alarm_get_state();
        
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
                mp3_cmd(CMD_STOP, 0);
                mp3_player_set_state(false);
                snooze_timeout = false;
                last_time_check = xTaskGetTickCount();
            }
            else if(brightness == DISPLAY_OFF){
                screen_wake(WAKE_PARTIAL);
            }

            //Clear radar
            snooze_radar_detected = false; 

            vTaskDelay(pdMS_TO_TICKS(500));
        }

        //Alarm Re-trigger after Snooze timeout
        snooze_timeout = (xTaskGetTickCount() - last_time_check) >= pdMS_TO_TICKS(SNOOZETIMER_MS);
        if(alarm_state == ALARM_SNOOZED && snooze_timeout == true){
            alarm_set_state(ALARM_TRIGGERED);
            screen_wake(WAKE_FULL);
            mp3_cmd(CMD_PLAY_W_INDEX, index_songs+1);
            mp3_player_set_state(true);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


