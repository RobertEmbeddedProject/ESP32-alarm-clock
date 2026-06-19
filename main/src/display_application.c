#include "master_alarm.h"
#include "mp3.h"
#include "rotary.h"
#include "display_application.h"
#include "display_ssd1309.h"
#include "display_graphics.h"
#include "graphics_bitmaps.h"
#include "songs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi.h"
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

//Screensaver Timers
#define DISPLAY_DIM_TIMEOUT_MS   3000   // ms DIM after idle
#define DISPLAY_VIEW_TIMEOUT_MS 4000   // ms after using snooze radar to check screensaver time
#define DISPLAY_OFF_TIMEOUT_MS 7000  // ms OFF after idle (set 0 to disable)
#define ALARM_WAKE_SLEEP_MS 5000  // ms OFF-after-5s window when alarm first rings
#define ALARM_DIM_PULSE_MS 5000

//Time Text Processing
static int display_clock_hour;
static int display_alarm_hour;
static float display_sleep_hours;
static char *alarm_ampm;
static char *clock_ampm;

//Task Notifications for handoff
extern TaskHandle_t display_task_t;
extern TaskHandle_t app_main_t;

//Screensaver and Dimming Properties
static bool screen_saver_initialized;
static TickType_t last_activity_ticks;
static enum brightness brightness = DISPLAY_BRIGHT;
static uint32_t wake_type = WAKE_FULL; //screen wake command selection

void display_task(void *arg){

    char wifi_text[32];
    char time_text[32];
    char large_time_text[6];
    char alarm_hour_text[32];
    char alarm_minute_text[32];
    char alarm_ampm_text[32];
    char sleep_text[32];
    char index_text[32];

    while(1){

        ssd1309_clear();

        bool music_playing = mp3_player_get_state();
        int index_songs = songs_get_index();

        //Push static labels and objects to display buffer
        update_display_info(wifi_text, time_text, alarm_hour_text, alarm_minute_text,
                                alarm_ampm_text, sleep_text, index_text, index_songs);

        snprintf(large_time_text, 6, "%.5s", time_text);

        //verify timeout for screensaver
        enum alarm alarm_state_snapshot = alarm_get_state();
        screen_saver(alarm_state_snapshot, &wake_type);

        if (wake_type == WAKE_PARTIAL) {
            ssd1309_draw_big_text(4, 8, large_time_text);
            ssd1309_draw_text(80, 6, alarm_ampm_text);
        }

        else{
            ssd1309_draw_text(64, 0, "WiFi: ");
            ssd1309_draw_text(104, 0, wifi_text);
            ssd1309_draw_text(20, 2, "Time:");
            ssd1309_draw_text(62, 2, time_text);
            ssd1309_draw_text(12, 3, "Alarm: "); 
            ssd1309_draw_text(0, 6, "Song Index:");
            if(alarm_get_state() != ALARM_CONFIG_WHITENOISE){
                ssd1309_draw_text(90, 6, index_text);
                ssd1309_draw_text(0, 7, songlist[index_songs]);
            }

            //invisible for 100ms, visible for 400ms, repeat every (total) 500ms
            bool blink = (xTaskGetTickCount() % pdMS_TO_TICKS(400)) < pdMS_TO_TICKS(100);

            switch (alarm_state_snapshot){
                case ALARM_IDLE:
                    ssd1309_draw_text(62, 3, "Not Set");
                    break;
                    
                case  ALARM_CONFIG_HOUR:
                    ssd1309_draw_text(78, 3, ":");
                    if(!blink){ssd1309_draw_text(62, 3, alarm_hour_text);}
                    ssd1309_draw_text(110, 3, alarm_ampm_text);
                    break;

                case  ALARM_CONFIG_MINUTE:
                    ssd1309_draw_text(62, 3, alarm_hour_text);
                    ssd1309_draw_text(78, 3, ":");
                    if(!blink){ssd1309_draw_text(86, 3, alarm_minute_text);}
                    ssd1309_draw_text(110, 3, alarm_ampm_text);
                    break;

                case  ALARM_CONFIG_WHITENOISE:
                    //first scan of config mode lets you pick no whitenoise, 
                    //otherwise the selection process of whitenoise_config begins
                    if(check_whitenoise_config()){
                        ssd1309_draw_text(90, 6, "  ");
                        if(!blink){ssd1309_draw_text(0, 7, "NO WHITENOISE");}
                        if(index_songs != 69){set_whitenoise_config(false);} // escape on movement
                    }
                    else{
                        if(!blink){
                            ssd1309_draw_text(90, 6, index_text);
                            ssd1309_draw_text(0, 7, songlist[index_songs]);
                        }
                    }
                    break;

                case  ALARM_ARMED:
                    ssd1309_draw_xbm(0, 1, 16, 16, image_clock_armed);
                    ssd1309_draw_text(18, 0, "Armed");
                    ssd1309_draw_text(62, 3, alarm_hour_text);
                    ssd1309_draw_text(78, 3, ":");
                    ssd1309_draw_text(86, 3, alarm_minute_text);
                    ssd1309_draw_text(110, 3, alarm_ampm_text);
                    if(display_sleep_hours == 1.0f){ssd1309_draw_text(12, 4, "Sleep:     hour");}
                    else{ssd1309_draw_text(12, 4, "Sleep:     hrs");}
                    ssd1309_draw_text(62, 4, sleep_text);
                    break;

                case  ALARM_TRIGGERED:
                    ssd1309_draw_xbm(0, 1, 16, 16, image_clock_triggered);
                    if(!blink){ssd1309_draw_text(18, 0, "WAKE");}
                    break;

                case  ALARM_SNOOZED:
                    ssd1309_draw_xbm(0, 1, 16, 16, image_clock_snoozed);
                    ssd1309_draw_text(16, 0, "SNOOZ");
                    //rfink add draw countdown timer of remaining snooze
                    break;

                default:
            }
            if(music_playing){ssd1309_draw_xbm(110, 42, 15, 14, image_speaker_sound_on);}
                         else{ssd1309_draw_xbm(110, 42, 15, 14, image_speaker_sound_off);}
        }

        if (brightness != DISPLAY_OFF) {ssd1309_display();}
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void format_AM_PM(int input_hour, int *display_hour, char **ampm){
    //Format Time as AM or PM

    if (input_hour == 0)
    {
        *display_hour = 12;
        *ampm = "AM";
    }
    else if (input_hour < 12)
    {
        *display_hour = input_hour;
        *ampm = "AM";
    }
    else if (input_hour == 12)
    {
        *display_hour = 12;
        *ampm = "PM";
    }
    else
    {
        *display_hour = input_hour - 12;
        *ampm = "PM";
    }
}

void update_display_info(char *wifi_text, char *time_text, char *alarm_hour_text,
                            char *alarm_minute_text, char *alarm_ampm_text, char *sleep_text,
                            char *index_text, int index_songs){
    
    //Time aquisition once per loop
    time_t now;
    struct tm current;
    time(&now);
    localtime_r(&now, &current);

    //Alarm Update
    int alarm_hour = alarm_hour_get_index();
    int alarm_min  = alarm_minute_get_index();

    //Sleep hours calculated from alarm
    int current_total_minutes = current.tm_hour * 60 + current.tm_min;
    int alarm_total_minutes = alarm_hour * 60 + alarm_min;                 //mins per day
    int minutes_until_alarm = (alarm_total_minutes - current_total_minutes + 1440) % 1440;
    display_sleep_hours = floorf(minutes_until_alarm / 6.0f) / 10.0f;

    format_AM_PM(alarm_hour, &display_alarm_hour, &alarm_ampm);
    format_AM_PM(current.tm_hour, &display_clock_hour, &clock_ampm);

    snprintf(wifi_text, 32, "%s",
        (wifi_is_connected() && wifi_time_is_synced()) ? "OK " : "Err");
    
    if (wifi_time_is_synced()) {
        snprintf(time_text, 32,
                "%2d:%02d %s",
                display_clock_hour,
                current.tm_min,
                clock_ampm);
        }
    else{
        snprintf(time_text, 32, "--:--");
    }

    snprintf(alarm_hour_text, 32,
            "%2d",
            display_alarm_hour);
    
    snprintf(alarm_minute_text, 32,
        "%02d", alarm_min);
    
    snprintf(alarm_ampm_text, 32,
        "%s", alarm_ampm);
            
    snprintf(sleep_text, 32,
            "%4.1f",
            display_sleep_hours);

    snprintf(index_text, 32, "%d", index_songs);
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

void progress_bar_fill(int start, int end, int lag_ms, int post_delay_ms){
    for(int i = start; i < end; i++){
        ssd1309_draw_xbm(1+i*2, 57, 2, 7, image_progressbar_fill);
        ssd1309_display();
        vTaskDelay(pdMS_TO_TICKS(lag_ms));
    }
    vTaskDelay(pdMS_TO_TICKS(post_delay_ms));
}

void screen_saver(enum alarm state, uint32_t *wake_type){
    TickType_t now = xTaskGetTickCount();
    uint32_t requested_wake_type;
    
    if (!screen_saver_initialized) {
        last_activity_ticks = now;
        screen_saver_initialized = true;
    }
    //Task Notification pass variable wake type (partial or full)
    if (xTaskNotifyWait(0, 0xFFFFFFFF, &requested_wake_type, 0) == pdTRUE) {
        last_activity_ticks = now;
        *wake_type = requested_wake_type;
    }

    enum brightness target;
    TickType_t elapsed = now - last_activity_ticks;

    //Display Brightness Change Conditions
    bool s_display_off_timeout = (elapsed >= pdMS_TO_TICKS(DISPLAY_OFF_TIMEOUT_MS));
    bool s_display_view_timeout = (elapsed >= pdMS_TO_TICKS(DISPLAY_VIEW_TIMEOUT_MS));
    bool s_display_dim_timeout = (elapsed >= pdMS_TO_TICKS(DISPLAY_DIM_TIMEOUT_MS));
    bool s_alarm_not_triggered = (state != ALARM_TRIGGERED);
    bool s_alarm_configuring = (state == ALARM_CONFIG_HOUR || state == ALARM_CONFIG_MINUTE || 
                                state == ALARM_CONFIG_WHITENOISE);
    bool s_partial_wakeup = (*wake_type == WAKE_PARTIAL);

    //Unless alarm is sounding or in config mode, turn off display after long amount of time
    if(((s_display_off_timeout && s_alarm_not_triggered) 
            || (s_display_view_timeout && s_partial_wakeup))
            && (!s_alarm_configuring)){

        target = DISPLAY_OFF;
    }
    //Unless alarm is sounding, or if the user wakes the display by waving,
    //  then dim the display after a short amount of time
    else if (((s_display_dim_timeout && s_alarm_not_triggered) || s_partial_wakeup)
            && (!s_alarm_configuring)) {
        target = DISPLAY_DIM;
    } 
    //Any other user interaction should keep the display awake
    else {
        target = DISPLAY_BRIGHT;
    }

    //Send buffered brightness state to the display
    if (target != brightness) {
        brightness = target;
        cmd_display_mode(brightness);
    }
}

enum brightness get_brightness(void){
    return brightness;
}

void screen_wake(uint32_t wake_type){
    if (display_task_t != NULL) {
        xTaskNotify(display_task_t, wake_type, eSetValueWithOverwrite);
    }
}

void display_splash(void){
    char wifi_text[32];
    char blank[32];

    ssd1309_clear();
    //rfink clean this up:
    update_display_info(wifi_text, blank, blank, blank, blank, blank, blank, 0);
    
    ssd1309_draw_text(64, 0, "WiFi:");
    ssd1309_draw_text(49, 1, "Connecting");
    ssd1309_draw_text(48, 6, "Loading");
    ssd1309_draw_xbm(0, 0, 49, 49, image_splash_clock);
    ssd1309_draw_xbm(1, 57, 126, 8, image_progressbar);
    ssd1309_display();
}
