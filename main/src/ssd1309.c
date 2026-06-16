#include "ssd1309.h"
#include "wifi.h"
#include "driver/gpio.h"
#include "graphics_smallfonts.h"
#include "graphics_bitmaps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" 
#include <math.h>

//I2C definitions
#define I2C_OLED_SDA   GPIO_NUM_21
#define I2C_OLED_SCL   GPIO_NUM_22
#define I2C_OLED_RESET GPIO_NUM_23
#define I2C_OLED_FREQ_HZ 400000 //Fast mode
#define OLED_ADDR      0x3C

//OLED IC Requirements
//Each buffer is 8 pixels vertically from bottom to top in order to create a "page"
#define OLED_WIDTH                  128 //pixels
#define OLED_HEIGHT                 64  //pixels
#define OLED_BUF_SIZE               (OLED_WIDTH * OLED_HEIGHT / 8) //1024 total pages
static uint8_t oled_buffer[OLED_BUF_SIZE];

//-------------------------------------------------------------------------------------------
//Dimmables Definition
#define DISPLAY_DIM_TIMEOUT_MS   3000   // ms DIM after idle
#define DISPLAY_OFF_TIMEOUT_MS 7000  // ms OFF after idle (set 0 to disable)
#define ALARM_WAKE_SLEEP_MS 5000  // ms OFF-after-5s window when alarm first rings
#define ALARM_DIM_PULSE_MS 5000

/********   Tunables    ************/
//Normal Display
#define VCOMH_BRIGHT      0x34   // ~0.78*VCC (datasheet reset value)
#define CONTRAST_BRIGHT   0x7F   // try 0x8F,0xFF, etc if needs to be brighter
//#define PRECHARGE_BRIGHT  0xF1   // phase2=0xF, phase1=0x1

//Dim Display
#define VCOMH_DIM         0x10   // ~0.64*VCC (darker)
#define CONTRAST_DIM      0x01   // 0x00..0x04 are very dark
//#define PRECHARGE_DIM     0x21   // both phases short -> reduces brightness
//-------------------------------------------------------------------------------------------

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

extern int index_songs;

//Alarm States and Properties
extern enum brightness brightness;
static bool screen_saver_initialized;
static TickType_t last_activity_ticks;

extern int index_alarm_hour;
extern int index_alarm_minute;
extern int alarm_hour;
extern int alarm_min;
extern int display_alarm_hour;
extern char *alarm_ampm;

extern int display_clock_hour;
extern char *clock_ampm;

extern float display_sleep_hours;

extern TaskHandle_t display_task_t;

//From the HAL
void I2C_init(void){
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0, //-1 is for auto-selection, use I2C_0 for display
        .scl_io_num = I2C_OLED_SCL, //GPIO assigned to SCL
        .sda_io_num = I2C_OLED_SDA, //GPIO assigned to SDA
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    /*RF: from HAL:
    create handle to represent bus configuration
    - allocate memory
    - claim I2C0 or I2C1
    - configure GPIO matrix routing
    - configure hardware registers
    - initialize synchronization primitives
    - install ISR
    - prepare transaction engine
    */
    
    //slave
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_ADDR,
        .scl_speed_hz = I2C_OLED_FREQ_HZ,
    };

    //master
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

}

void OLED_cmd(uint8_t cmd){
    uint8_t buffer[2] = {0x00, cmd}; // 0x00 = command stream (not data)

    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, buffer, 2, -1));
    //RF from HAL i2c_master_transmit(i2c_master_dev_handle_t i2c_dev, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
    /*RF Notes:

    buffer[2] becomes &buffer[0] when passed in, which is type uint8_t *

    Array variable:               Pointer:
    owns storage                  references storage
    */
}

void OLED_init(void){
    I2C_init();

    //see pg 26 of data sheet for power-up diagram
    const uint8_t init_sequence[] = {
    0xAE, // Display off
    0xD5, 0x80, // Set display clock
    0xA8, 0x3F, // Multiplex
    0xD3, 0x00, // Display offset
    0x40, // Start line
    0x8D, 0x14, // Charge pump
    0x20, 0x00, // Memory mode: horizontal
    0xA1, // Seg remap
    0xC8, // COM scan dec
    0xDA, 0x12, // COM pins
    0x81, 0x0F, // Contrast
    0xD9, 0xF1, // Precharge
    0xDB, 0x40, // VCOM detect
    0xA4, // Resume RAM
    0xA6, // Normal display
    0xAF  // Display ON
    };

    for(uint8_t i=0; i<sizeof(init_sequence); i++){
        OLED_cmd(init_sequence[i]);
    }
}


void ssd1309_clear(void) {
    memset(oled_buffer, 0x00, sizeof(oled_buffer));
}

void ssd1309_fill(void) {
    memset(oled_buffer, 0xFF, sizeof(oled_buffer));
}

void ssd1309_draw_pixel(int x, int y, bool on)
{
    //bounds checking, prevents memory corruption
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }

    //pixel index simplifier (SSD1309 specific)
    uint16_t index = x + (y / 8) * OLED_WIDTH;
    //create a mask (can't specify single pixels to SSD1309, must have a buffered byte
    uint8_t mask = 1 << (y % 8);

    if (on) {
        oled_buffer[index] |= mask;
    } else {
        oled_buffer[index] &= ~mask;
    }
}


//hex bitmap
void ssd1309_draw_xbm(int x, int y, int w, int h, const uint8_t *bitmap)
{
    int byte_width = (w + 7) / 8;

    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            int byte_index = yy * byte_width + (xx / 8);
            uint8_t bit_mask = 1 << (xx & 7);

            if (bitmap[byte_index] & bit_mask) {
                ssd1309_draw_pixel(x + xx, y + yy, true);
            }
        }
    }
}

void progress_bar_fill(int start, int end, int lag_ms, int post_delay_ms){
    for(int i = start; i < end; i++){
        ssd1309_draw_xbm(1+i*2, 57, 2, 7, image_progressbar_fill);
        ssd1309_display();
        vTaskDelay(pdMS_TO_TICKS(lag_ms));
    }
    vTaskDelay(pdMS_TO_TICKS(post_delay_ms));
}
    

void ssd1309_display(void) {

    for (uint8_t page = 0; page < 8; page++) {
        OLED_cmd(0xB0 + page);
        OLED_cmd(0x00);
        OLED_cmd(0x10);
        

        uint8_t tx[OLED_WIDTH + 1];
        tx[0] = 0x40;  // SSD1309 control byte: following bytes are display data

        memcpy(&tx[1], &oled_buffer[OLED_WIDTH * page], OLED_WIDTH);

        ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, tx, sizeof(tx), 1000));
    }
}

void ssd1309_draw_char(int x, int y, char c) {
    if (c < 32 || c > 127) return;
    const uint8_t *glyph = font8x8_basic[c - 32];

    for (int col = 0; col < 8; col++) {
    uint8_t column_data = 0;
    for (int row = 0; row < 8; row++) {
        column_data |= ((glyph[row] >> col) & 0x01) << row;
    }

    // Flip horizontal draw order:
    int index = x + (y * 128) + (7 - col);  // Flip col
    if (index < OLED_BUF_SIZE) {
        oled_buffer[index] = column_data;
    }
  }
}

void ssd1309_draw_text(int x, int y, const char *text) {
    while (*text) {
        ssd1309_draw_char(x, y, *text++);
        x += 8;
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
                            char *alarm_minute_text, char *sleep_text, char *index_text){
    
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

    display_sleep_hours =
    floorf(minutes_until_alarm / (float)30) / 2.0f;

    format_AM_PM(alarm_hour, &display_alarm_hour, &alarm_ampm);
    format_AM_PM(current.tm_hour, &display_clock_hour, &clock_ampm);

    snprintf(wifi_text, 32,"%s",
            wifi_is_connected() ? "OK " : "Err");
    
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
        "%02d %s",
        alarm_min,
        alarm_ampm);
            
    snprintf(sleep_text, 32,
            "%4.1f",
            display_sleep_hours);

    snprintf(index_text, 32, "%d", index_songs);
}

void cmd_display_mode(enum brightness state)
{
    switch (state) {
        case DISPLAY_OFF:
            OLED_cmd(0xAE);   // Display OFF
            break;

        case DISPLAY_DIM:
            OLED_cmd(0xAF);   // Display ON
            OLED_cmd(0xDB);   // Set VCOMH deselect level
            OLED_cmd(0x00);   // Lowest VCOMH setting
            OLED_cmd(0xD9);   // Set pre-charge period
            OLED_cmd(0x01);   // Phase 2 = 0, phase 1 = 1
            OLED_cmd(0x81);   // Set contrast
            OLED_cmd(0x01);   // Very low contrast
            break;

        case DISPLAY_BRIGHT:
            OLED_cmd(0xAF);   // Display ON
            OLED_cmd(0xDB);   // Set VCOMH deselect level
            OLED_cmd(0x3C);   // Bright VCOMH setting
            OLED_cmd(0xD9);   // Set pre-charge period
            OLED_cmd(0xF1);   // Phase 2 = 15, phase 1 = 1
            OLED_cmd(0x81);   // Set contrast
            OLED_cmd(0x0F);   // Normal contrast from your init sequence
            break;

        default:
            break;
    }
}

void screen_saver(enum alarm *state){
    *state = alarm_state; //snapshot for one-scan consistency
    TickType_t now = xTaskGetTickCount();

    if (!screen_saver_initialized) {
        last_activity_ticks = now;
        screen_saver_initialized = true;
    }
    if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
        last_activity_ticks = now;
    }

    TickType_t elapsed = now - last_activity_ticks;
    enum brightness target;
    if (elapsed >= pdMS_TO_TICKS(DISPLAY_OFF_TIMEOUT_MS)) {
        target = DISPLAY_OFF;
    } 
    else if (elapsed >= pdMS_TO_TICKS(DISPLAY_DIM_TIMEOUT_MS)) {
        target = DISPLAY_DIM;
    } 
    else {
        target = DISPLAY_BRIGHT;
    }
    if (target != brightness) {
        brightness = target;
        cmd_display_mode(brightness);
    }
}

void screen_activity(void){
    if (display_task_t != NULL) {
        xTaskNotifyGive(display_task_t);
    }
}
