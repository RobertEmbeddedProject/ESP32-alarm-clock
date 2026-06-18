#include "display_ssd1309.h"
#include "driver/gpio.h"
#include <string.h>

//I2C definitions
#define I2C_OLED_SDA   GPIO_NUM_21
#define I2C_OLED_SCL   GPIO_NUM_22
#define I2C_OLED_RESET GPIO_NUM_23
#define I2C_OLED_FREQ_HZ 400000 //Fast mode
#define OLED_ADDR      0x3C

uint8_t oled_buffer[OLED_BUF_SIZE];

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

void ssd1309_fill(void) {
    memset(oled_buffer, 0xFF, sizeof(oled_buffer));
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

