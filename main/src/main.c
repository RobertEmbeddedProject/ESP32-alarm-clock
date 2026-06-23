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


    
    //temp snooze button init
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_18),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);


    mp3_init();
    xTaskCreate(mp3_task, "mp3_task", 3072, NULL, 3, NULL);
    xTaskCreate(song_playback_task, "song_playback_task", 3072, NULL, 2, &song_playback_t);

    radar_snooze_init();

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // wait until splash task is done
    xTaskCreate(snooze_task, "snooze_task", 2048, NULL, 3, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 4, &display_task_t);
    xTaskCreate(alarm_task, "alarm_task", 2048, NULL, 5, NULL);
   xTaskCreate(rotary_task, "rotary_task", 2048, NULL, 7, NULL);
    ESP_LOGI("HEAP", "Heap Remaining: %lu", esp_get_free_heap_size());

}
