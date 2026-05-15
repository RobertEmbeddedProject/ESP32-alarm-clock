#include "ssd1309.h"

//I2C definitions
#define I2C_OLED_SDA   21
#define I2C_OLED_SCL   22
#define I2C_OLED_RESET 23
#define I2C_OLED_FREQ_HZ 400000 //Fast mode
#define OLED_ADDR      0x3C

//OLED IC Requirements
//Each buffer is 8 pixels vertically from bottom to top in order to create a "page"
#define OLED_WIDTH                  128
#define OLED_HEIGHT                 64
#define OLED_BUF_SIZE               (OLED_WIDTH * OLED_HEIGHT / 8)
static uint8_t oled_buffer[OLED_BUF_SIZE];

static i2c_master_dev_handle_t dev_handle;


//From the HAL
void I2C_init(void){
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1, //default value for auto-selection (HAL) 
        .scl_io_num = I2C_OLED_SCL, //GPIO assigned to SCL
        .sda_io_num = I2C_OLED_SDA, //GPIO assigned to SDA
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
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
void ssd1309_draw_hline(int x_start, int x_end, int y) {
    if (y < 0 || y >= OLED_HEIGHT) return;
    if (x_start > x_end) {
        int tmp = x_start;
        x_start = x_end;
        x_end = tmp;
    }

    if (x_start < 0) x_start = 0;
    if (x_end >= OLED_WIDTH) x_end = OLED_WIDTH - 1;

    int page = y / 8;
    uint8_t bit_mask = 1 << (y % 8);

    for (int x = x_start; x <= x_end; x++) {
        int index = page * OLED_WIDTH + x;
        if (index >= 0 && index < OLED_BUF_SIZE) {
            oled_buffer[index] |= bit_mask;
        }
    }
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
