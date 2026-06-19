#include "main.h"
#include <time.h>
#include <esp_log.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "master_alarm.h"
#include "snooze.h"
#include "display_ssd1309.h"
#include "display_graphics.h"
#include "display_application.h"
#include "graphics_bitmaps.h"
#include "rotary.h"
#include "songs.h"
#include "mp3.h"
#include "wifi.h"
#include "radar_snooze.h"

//task notifications
static TaskHandle_t splash_task_t = NULL;
TaskHandle_t app_main_t = NULL;
TaskHandle_t display_task_t = NULL;
TaskHandle_t song_playback_t = NULL;


void app_main(void)
{
    app_main_t = xTaskGetCurrentTaskHandle();

    OLED_init();
    ssd1309_clear();
    ssd1309_display();

    display_splash();
    xTaskCreate(splash_load_task, "splash load task", 2048, NULL, 5, &splash_task_t);

    wifi_init();
    xTaskCreate(wifi_reconnect_task, "wifi_reconnect", 2048, NULL, 3, NULL);
    xTaskNotifyGive(splash_task_t);

    rotary_init_songs_and_alarm();

    mp3_init();
    radar_snooze_init();

    vTaskDelay(pdMS_TO_TICKS(500)); //delay must be 500<
    mp3_cmd(CMD_SET_VOLUME, 30);

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // wait until splash task is done
    xTaskCreate(snooze_task, "snooze_task", 2048, NULL, 3, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 4, &display_task_t);
    xTaskCreate(alarm_task, "alarm_task", 2048, NULL, 5, NULL);
    xTaskCreate(song_playback_task, "song_playback_task", 2048, NULL, 6, &song_playback_t);
    xTaskCreate(rotary_task, "rotary_task", 2048, NULL, 7, NULL);

}
