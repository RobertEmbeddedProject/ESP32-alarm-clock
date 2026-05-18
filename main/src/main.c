
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"   //RTOS implement for time vTaskDelay commands
#include "freertos/task.h"       //Task management API xTaskCreate(), vTaskDelete(), vTaskDelay()
#include "driver/gpio.h"
#include "main.h"

#include "ssd1309.h"

#define testdelay 500 //milliseconds


void app_main(void)
{
    OLED_init();

    while (1) {
        
    vTaskDelay(pdMS_TO_TICKS(testdelay/2));
    ssd1309_clear();
    ssd1309_display();
    vTaskDelay(pdMS_TO_TICKS(testdelay));
    ssd1309_draw_text(20, 2, "test");
    ssd1309_display();

    }
}


