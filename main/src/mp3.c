#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <esp_log.h>
#include "mp3.h"
#include "rotary.h"
#include "master_alarm.h"
#include "display_application.h"

#define DFPLAYER_RESET_GPIO     4

//UART settings
#define UART_PORT_NUM      UART_NUM_2
#define UART_TX_PIN        17
#define UART_RX_PIN        16
#define UART_BUF_SIZE      1024
#define UART_BAUD          9600

//MP3 Player
static volatile bool music_playing = 0;
static volatile int song_playing = 999;

// DFPlayer protocol frame: 0x7E FF 06 <cmd> 00 <hi> <lo> <chkH> <chkL> 0xEF
static void df_send_packet(uint8_t cmd, uint16_t param)
{
    uint8_t pkt[10];
    //this has to be calculated, because the DFplayer expects a different checksum per command
    uint16_t checksum = 0 - (0xFF + 0x06 + cmd + 0x00 + (param >> 8) + (param & 0xFF));

    pkt[0] = 0x7E;
    pkt[1] = 0xFF;
    pkt[2] = 0x06;
    pkt[3] = cmd;
    pkt[4] = 0x00;               // no feedback
    pkt[5] = (param >> 8) & 0xFF;
    pkt[6] = (param     ) & 0xFF;
    pkt[7] = (checksum >> 8) & 0xFF;
    pkt[8] = (checksum     ) & 0xFF;
    pkt[9] = 0xEF;

    uart_write_bytes(UART_PORT_NUM, (const char*)pkt, sizeof(pkt));
    vTaskDelay(pdMS_TO_TICKS(20));
}

void mp3_cmd(int8_t command, int16_t dat){
    df_send_packet((uint8_t)command, (uint16_t)dat);
}

void mp3_init(void){

// DFPlayer RESET GPIO (transistor circuit)
    gpio_config_t io_rst = {
        .pin_bit_mask = (1ULL << DFPLAYER_RESET_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = 0,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&io_rst);
    gpio_set_level(DFPLAYER_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(DFPLAYER_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(400)); // delay must be 400<

    //from HAL
    uart_config_t uart_config = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    //UART pin assignment
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE, 0, 0, NULL, 0);

    // DFPlayer setup
    mp3_cmd(CMD_SEL_DEV, DEV_TF); //tell DFplayer to use SD card
    vTaskDelay(pdMS_TO_TICKS(50));
    mp3_cmd(CMD_SET_VOLUME, 30); // start at low volume
    mp3_cmd(CMD_SET_PRESET, 2);
}

void song_playback_task(void *args){
    ESP_LOGI("SONG", "song_playback_task started");

    while (1) {

        int index_songs = songs_get_index();

        //Normal playback for listening to music
        if (gpio_get_level(GPIO_NUM_39) == 0 && song_playing != index_songs && alarm_get_state() != ALARM_TRIGGERED) {
            mp3_cmd(CMD_PLAY_W_INDEX, index_songs+1);
            music_playing = 1;
            song_playing = index_songs;
            screen_wake(WAKE_FULL);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
       else if (gpio_get_level(GPIO_NUM_39) == 0 && music_playing == 1 && index_songs == song_playing){
            mp3_cmd(CMD_STOP, 0);
            music_playing = 0;
            song_playing  = 999; 
            screen_wake(WAKE_FULL);
            vTaskDelay(pdMS_TO_TICKS(500));
       }

       //Next song automatically if casually listening to music
       if(true){}

       //White Noise repeats until alarm is triggered


       //Alarm Triggered playback
       if(ulTaskNotifyTake(pdTRUE, 0) > 0){
            alarm_set_state(ALARM_TRIGGERED);
            screen_wake(WAKE_FULL);
            mp3_cmd(CMD_PLAY_W_INDEX, index_songs+1);
            music_playing = 1;
       }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool mp3_player_get_state(void)
{
    return music_playing;
}

void mp3_player_set_state(bool state)
{
    music_playing = state;
}


