#include "mp3.h"
#include "rotary.h"
#include "master_alarm.h"
#include "display_application.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <esp_log.h>
#include <stdbool.h>
#include <stdint.h>

//delete for mini alarm clock
#define DFPLAYER_RESET_GPIO     4

//UART settings
#define UART_PORT_NUM      UART_NUM_2
#define UART_TX_PIN        17
#define UART_RX_PIN        16
#define UART_BUF_SIZE      1024
#define UART_BAUD          9600

//For Queue, commands from other tasks
static QueueHandle_t mp3_queue;

//MP3 Player
static volatile bool music_playing = false;
#define SONG_INDEX_NONE  (-1)
static volatile int song_playing = SONG_INDEX_NONE;

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

//rfink delete for mini alarm clock
/*
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
*/ 

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
    mp3_cmd(CMD_SET_VOLUME, 30); //start at full volume
    mp3_cmd(CMD_SET_PRESET, 2); //use rock preset (see header)

    //Queue for other tasks to send commands also
    mp3_queue = xQueueCreate(10, sizeof(mp3_request_t));
    configASSERT(mp3_queue != NULL);
}

//For Command Queue
bool mp3_request(mp3_req_type_t type, uint16_t value)
{
    mp3_request_t req = {
        .type = type,
        .value = value
    };

    if (xQueueSend(mp3_queue, &req, pdMS_TO_TICKS(50)) != pdPASS) {
        ESP_LOGE("MP3", "MP3 command queue full");
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------


void song_playback_task(void *args){
    ESP_LOGI("SONG", "song_playback_task started");

    while (1) {

        enum alarm alarm_state = alarm_get_state();

        int index_songs = songs_get_index();
        uint16_t finished_track_index = 999;
        bool track_completed = mp3_track_finished(&finished_track_index);

        bool play_button_pressed = (gpio_get_level(GPIO_NUM_39) == 0);

        //Normal playback for listening to music
        if (play_button_pressed && 
                alarm_state != ALARM_TRIGGERED && alarm_state != ALARM_SNOOZED) {
            if (music_playing && song_playing == index_songs) {
                mp3_request(MP3_REQ_STOP, 0);
            } else {
                mp3_request(
                    MP3_REQ_PLAY_INDEX,
                    index_songs + 1
                );
            }
            screen_wake(WAKE_FULL);
            vTaskDelay(pdMS_TO_TICKS(500));
        }

       //Play next song automatically if casually listening to music
       if (track_completed && alarm_state != ALARM_ARMED){
            vTaskDelay(pdMS_TO_TICKS(100)); //provide a small gap between songs
            mp3_request(MP3_REQ_PLAY_INDEX, finished_track_index+1);
            songs_set_index(finished_track_index);
        screen_wake(WAKE_FULL);
        }
        

       //White Noise repeats same track until alarm is triggered
        else if (track_completed && alarm_state == ALARM_ARMED){
            mp3_request(MP3_REQ_PLAY_INDEX, finished_track_index);
        }

       //Alarm Triggered playback
       if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            alarm_set_state(ALARM_TRIGGERED);
            screen_wake(WAKE_FULL);
            mp3_request(MP3_REQ_START_ALARM, index_songs + 1);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool mp3_track_finished(uint16_t *track_out){
    static uint8_t frame[DFPLAYER_FRAME_SIZE];
    static size_t frame_index = 0;

    uint8_t byte;

    while (uart_read_bytes(UART_PORT_NUM, &byte, 1, 0) == 1) {

        /* Ignore data until the beginning of a frame is found. */
        if (frame_index == 0 && byte != 0x7E) {
            continue;
        }

        frame[frame_index++] = byte;

        if (frame_index < DFPLAYER_FRAME_SIZE) {
            continue;
        }

        /* A complete 10-byte frame has been collected. */
        frame_index = 0;

        uint16_t received_checksum =
            ((uint16_t)frame[7] << 8) | frame[8];

        uint16_t expected_checksum =
            0 - (frame[1] +
                 frame[2] +
                 frame[3] +
                 frame[4] +
                 frame[5] +
                 frame[6]);

        bool valid_frame =
            frame[0] == 0x7E &&
            frame[1] == 0xFF &&
            frame[2] == 0x06 &&
            frame[9] == 0xEF &&
            received_checksum == expected_checksum;

        if (!valid_frame) {
            continue;
        }

        if (frame[3] == DFPLAYER_TF_FINISHED_CMD) {
            if (track_out != NULL) {
                *track_out =
                    ((uint16_t)frame[5] << 8) | frame[6];
            }

            return true;
        }
    }

    return false;
}



void mp3_task(void *args)
{
    ESP_LOGI("MP3", "mp3_task started");

    bool alarm_ramp_active = false;
    int ramp_volume = 1;
    TickType_t last_ramp_tick = 0;

    mp3_request_t req;

    while (1) {
        if (xQueueReceive(mp3_queue, &req, pdMS_TO_TICKS(20)) == pdTRUE) {
            switch (req.type) {
                case MP3_REQ_PLAY_INDEX:
                    /*
                     * Normal listening/white-noise playback should cancel
                     * alarm ramping and use normal volume.
                     */
                    alarm_ramp_active = false;

                    mp3_cmd(CMD_SET_VOLUME, 30);
                    mp3_cmd(CMD_PLAY_W_INDEX, req.value);

                    music_playing = true;
                    song_playing = (int)req.value - 1;
                    break;

                case MP3_REQ_STOP:
                    alarm_ramp_active = false;

                    mp3_cmd(CMD_STOP, 0);

                    music_playing = false;
                    song_playing = SONG_INDEX_NONE;
                    break;

                case MP3_REQ_SET_VOLUME:
                    mp3_cmd(CMD_SET_VOLUME, req.value);
                    break;

                case MP3_REQ_START_ALARM:
                    alarm_ramp_active = true;
                    ramp_volume = 1;
                    last_ramp_tick = xTaskGetTickCount();

                    mp3_cmd(CMD_SET_VOLUME, ramp_volume);
                    mp3_cmd(CMD_PLAY_W_INDEX, req.value);

                    music_playing = true;
                    song_playing = (int)req.value - 1;
                    break;

                case MP3_REQ_SNOOZE_STOP:
                    alarm_ramp_active = false;

                    mp3_cmd(CMD_STOP, 0);

                    music_playing = false;
                    song_playing = SONG_INDEX_NONE;
                    break;

                default:
                    ESP_LOGW("MP3", "Unknown MP3 request: %d", req.type);
                    break;
            }
        }

        if (alarm_ramp_active &&
            (xTaskGetTickCount() - last_ramp_tick) >=
                pdMS_TO_TICKS(500)) {

            last_ramp_tick = xTaskGetTickCount();

            if (ramp_volume < 30) {
                ramp_volume++;
                mp3_cmd(CMD_SET_VOLUME, ramp_volume);
            } else {
                alarm_ramp_active = false;
            }
        }
    }
}

bool mp3_is_music_playing(void)
{
    return music_playing;
}

void mp3_set_music_playing(bool state)
{
    music_playing = state;
}


