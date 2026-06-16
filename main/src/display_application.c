#include "display_application.h"
#include "display_ssd1309.h"
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

extern volatile int index_alarm_hour;
extern volatile int index_alarm_minute;
extern volatile int alarm_hour;
extern volatile int alarm_min;
extern int display_alarm_hour;
extern char *alarm_ampm;

extern int display_clock_hour;
extern char *clock_ampm;

extern float display_sleep_hours;

extern TaskHandle_t display_task_t;

extern volatile int index_songs;

//Alarm States and Properties
extern enum brightness brightness;
static bool screen_saver_initialized;
static TickType_t last_activity_ticks;


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
    bool s_alarm_configuring = (state == ALARM_CONFIG_HOUR || state == ALARM_CONFIG_MINUTE);
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

void screen_wake(uint32_t wake_type){
    if (display_task_t != NULL) {
        xTaskNotify(display_task_t, wake_type, eSetValueWithOverwrite);
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
                            char *alarm_minute_text, char *alarm_ampm_text, char *sleep_text, char *index_text){
    
    //Time aquisition once per loop
    time_t now;
    struct tm current;
    time(&now);
    localtime_r(&now, &current);

    //Alarm Update
    alarm_hour = index_alarm_hour;
    alarm_min = index_alarm_minute;

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
