#include "ssd1309.h"
#include "wifi.h"
#include "globals.h"
#include "driver/gpio.h"
#include "graphics_smallfonts.h"
#include "graphics_bitmaps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" 

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

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

extern int index_songs;

extern int index_alarm;
extern int alarm_hour;
extern int alarm_min;
extern int display_alarm_hour;
extern char *alarm_ampm;

extern int display_clock_hour;
extern char *clock_ampm;


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
        ssd1309_draw_xbm(56+i*2, 40, 2, 8, image_progressbar_fill);
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

void update_display_info(char *wifi_text, char *time_text, char *alarm_text, char *index_text){
    
    //Time aquisition once per loop
    time_t now;
    struct tm current;
    time(&now);
    localtime_r(&now, &current);

    //Alarm Update
    alarm_min = index_alarm * 5 % 60; //5 min increments
    alarm_hour = index_alarm * 5 / 60;

    format_AM_PM(alarm_hour, &display_alarm_hour, &alarm_ampm);
    format_AM_PM(current.tm_hour, &display_clock_hour, &clock_ampm);

    snprintf(wifi_text, 32,"%s",
            wifi_is_connected() ? "OK " : "Err");

    snprintf(time_text, 32,
            "%2d:%02d %s",
            display_clock_hour,
            current.tm_min,
            clock_ampm);

    snprintf(alarm_text, 32,
            "%2d:%02d %s",
            display_alarm_hour,
            alarm_min,
            alarm_ampm);        

    snprintf(index_text, 32, "%d", index_songs);
}
