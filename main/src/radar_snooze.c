#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include <stdatomic.h>
#include "radar_snooze.h"

// ---------- Wiring / UART config ----------
// NOTE: UART0 shares pins with the default serial console.
// LD2410C default: 256000 8N1 (TTL 3.3V I/O; module Vcc is 5V) 
// Ref: official protocol docs & community implementations.
#define RADAR_UART_NUM        UART_NUM_1
#define RADAR_UART_TX         UART_PIN_NO_CHANGE //No TX needed
#define RADAR_UART_RX         GPIO_NUM_35        //RX for motion output
#define RADAR_UART_BAUD       256000
#define RADAR_RXBUF_SIZE      4096

// ---------- LD2410C framing (data stream) ----------
static const uint8_t DATA_HDR[4] = {0xF4,0xF3,0xF2,0xF1}; // data frame header
static const uint8_t DATA_FTR[4] = {0xF8,0xF7,0xF6,0xF5}; // data frame footer
// Inside the frame there is a sub-block delimited by 0xAA ... 0x55.
// Offsets within that AA..55 block (from ESPHome’s public implementation):
enum {
    PD_DATA_TYPES        = 6,
    PD_TARGET_STATES     = 8,  // bit0 = moving, bit1 = still
    PD_MOVING_TARGET_LO  = 9,
    PD_MOVING_TARGET_HI  = 10,
    PD_MOVING_ENERGY     = 11,
    PD_STILL_TARGET_LO   = 12,
    PD_STILL_TARGET_HI   = 13,
    PD_STILL_ENERGY      = 14,
};

static const char *TAG = "RADAR";

// Flags exposed to the rest of the app
static _Atomic bool s_motion     = false;
static _Atomic bool s_stationary = false;

static bool s_dump_raw = false;

// Small FIFO for framing
static uint8_t s_fifo[RADAR_RXBUF_SIZE];
static size_t  s_len = 0;

static int find_bytes(const uint8_t *buf, size_t len, const uint8_t *pat, size_t patlen) {
    if (patlen == 0 || len < patlen) return -1;
    for (size_t i = 0; i <= len - patlen; ++i) {
        if (memcmp(&buf[i], pat, patlen) == 0) return (int)i;
    }
    return -1;
}

static void consume(size_t n) {
    if (n >= s_len) { s_len = 0; return; }
    memmove(s_fifo, s_fifo + n, s_len - n);
    s_len -= n;
}

static void parse_frame(const uint8_t *frame, size_t framelen) {
    if (framelen < 10) return;

    uint16_t payload_len = (uint16_t)frame[4] | ((uint16_t)frame[5] << 8);
    if (payload_len + 10 != framelen) return; // hdr(4)+len(2)+payload+ftr(4)

    const uint8_t *p = frame + 6;       // payload start
    uint16_t plen    = payload_len;

    if (plen < 5) return;               // need type+AA+...+55+cal
    uint8_t dtype = p[0];               // 0x02=basic, 0x01=engineering
    if (p[1] != 0xAA) return;

    // find 0x55 (tail) before the last byte (calibration 0x00)
    int tail = -1;
    for (uint16_t i = 2; i + 1 < plen; ++i) {
        if (p[i] == 0x55) { tail = i; break; }
    }
    if (tail < 0) return;

    const uint8_t *td = p + 2;          // target-data begins here
    size_t tlen = (size_t)(tail - 2);

    // First byte of target-data is Target Status (Table 12/13)
    if (tlen < 1) return;
    uint8_t tstate = td[0];
    bool moving    = (tstate & 0x01) != 0;   // 0x01 moving
    bool still     = (tstate & 0x02) != 0;   // 0x02 stationary

    bool prev_m = atomic_load(&s_motion);
    bool prev_s = atomic_load(&s_stationary);
    atomic_store(&s_motion, moving);
    atomic_store(&s_stationary, still);

    if (moving != prev_m || still != prev_s) {
        if (moving && still)      ESP_LOGI(TAG, "Target state: MOVING + STATIONARY");
        else if (moving)          ESP_LOGI(TAG, "Target state: MOVING (motion)");
        else if (still)           ESP_LOGI(TAG, "Target state: STATIONARY (presence)");
        else                      ESP_LOGI(TAG, "Target state: NONE");
    }

    // Optional: parse distances/energies if present (basic mode packs:
    // status(1), mv_dist(2), mv_energy(1), st_dist(2), st_energy(1), det_dist(2))
    if (s_dump_raw && tlen >= 9) {
        uint16_t mv_d = (uint16_t)td[1] | ((uint16_t)td[2] << 8);
        uint8_t  mv_e = td[3];
        uint16_t st_d = (uint16_t)td[4] | ((uint16_t)td[5] << 8);
        uint8_t  st_e = td[6];
        uint16_t detd = (uint16_t)td[7] | ((uint16_t)td[8] << 8);
        ESP_LOGD(TAG, "mv:%ucm/%u  st:%ucm/%u  det:%ucm", mv_d, mv_e, st_d, st_e, detd);
    }
}

static void process_bytes(const uint8_t *data, size_t n) {
    // Append to FIFO
    if (n > sizeof(s_fifo) - s_len) n = sizeof(s_fifo) - s_len;
    memcpy(s_fifo + s_len, data, n);
    s_len += n;

    // Extract complete frames
    while (1) {
        int h = find_bytes(s_fifo, s_len, DATA_HDR, sizeof(DATA_HDR));
        if (h < 0) {
            // keep last 3 bytes to allow header overlap
            if (s_len > 3) { consume(s_len - 3); }
            return;
        }
        if ((size_t)h + 6 > s_len) {
            // wait for more bytes (need length too)
            if (h > 0) consume(h);
            return;
        }

        uint16_t payload_len = (uint16_t)s_fifo[h+4] | ((uint16_t)s_fifo[h+5] << 8);
        size_t frame_len = 4 + 2 + payload_len + 4; // hdr + len + payload + ftr
        if ((size_t)h + frame_len > s_len) {
            // incomplete frame yet
            if (h > 0) consume(h);
            return;
        }

        // Validate footer
        if (memcmp(s_fifo + h + 4 + 2 + payload_len, DATA_FTR, sizeof(DATA_FTR)) != 0) {
            // bad framing; drop header byte and resync
            consume(h + 1);
            continue;
        }

        // We have a whole frame
        if (s_dump_raw) {
            char line[200];
            size_t to = (frame_len < 64 ? frame_len : 64);
            size_t p  = 0;
            for (size_t i = 0; i < to && p + 3 < sizeof(line); ++i) {
                p += snprintf(line + p, sizeof(line) - p, "%02X ", s_fifo[h + i]);
            }
            ESP_LOGD(TAG, "Frame[%u]: %s%s", (unsigned)frame_len, line, (frame_len>to?"...":""));
        }

        parse_frame(s_fifo + h, frame_len);
        consume(h + frame_len);
    }
}

static void radar_task(void *arg) {
    (void)arg;
    const uart_config_t cfg = {
        .baud_rate = RADAR_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk= UART_SCLK_APB
    };
    uart_param_config(RADAR_UART_NUM, &cfg);
    uart_set_pin(RADAR_UART_NUM, RADAR_UART_TX, RADAR_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // Big enough RX buffer to hold a couple frames
    uart_driver_install(RADAR_UART_NUM, RADAR_RXBUF_SIZE, 0, 0, NULL, 0);

    ESP_LOGI(TAG, "UART%d up for LD2410C @ %d (TX=%d, RX=%d)",
         (int)RADAR_UART_NUM, RADAR_UART_BAUD,
         (int)RADAR_UART_TX, (int)RADAR_UART_RX);


    uint8_t buf[256];
    while (1) {
        int n = uart_read_bytes(RADAR_UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(50));
        if (n > 0) process_bytes(buf, (size_t)n);
        // Yield a bit even if nothing arrived
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ---------- Public API ----------
void radar_snooze_init(void) {
    static bool started = false;
    if (started) return;
    started = true;
    xTaskCreate(radar_task, "radar_uart", 4096, NULL, 5, NULL);
}

void radar_debug_enable_raw_dump(bool on) { s_dump_raw = on; }
bool radar_get_motion(void)     { return atomic_load(&s_motion); }
bool radar_get_stationary(void) { return atomic_load(&s_stationary); }
