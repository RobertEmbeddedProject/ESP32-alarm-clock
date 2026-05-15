
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"   //RTOS implement for time vTaskDelay commands
#include "freertos/task.h"       //Task management API xTaskCreate(), vTaskDelete(), vTaskDelay()
#include "driver/gpio.h"
#include "main.h"

#include "ssd1309.h"


void app_main(void){
    
    OLED_init();
    //ssd1309_clear();
    ssd1309_draw_hline(12,25,5);
    ssd1309_display();

    while(1){

    }
}

