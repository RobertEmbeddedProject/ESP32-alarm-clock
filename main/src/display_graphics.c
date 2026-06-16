#include "display_graphics.h"
#include "display_ssd1309.h"
#include "graphics_smallfonts.h"
#include "graphics_bigfonts.h"
#include "graphics_bitmaps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void ssd1309_draw_pixel(int x, int y, bool on){
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
void ssd1309_draw_xbm(int x, int y, int w, int h, const uint8_t *bitmap){
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

void ssd1309_draw_char(int x, int y, char c) {
    if (c < 32 || c > 127) return;
    const uint8_t *glyph = font8x8_basic[c - 32];

    for (int col = 0; col < 8; col++) {
    uint8_t column_data = 0;
    for (int row = 0; row < 8; row++) {
        column_data |= ((glyph[row] >> col) & 0x01) << row;
    }

    // Flip horizontal draw order:
    int index = x + (y * OLED_WIDTH) + (7 - col);  // Flip col
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

void ssd1309_draw_big_char(int x, int y, char c){
    if (c < 32 || c > 127) return;

    const uint8_t *glyph = font24x32_basic[c - 32];

    for (int row = 0; row < 32; row++) {

        const uint8_t *row_data = &glyph[row * 3];

        for (int byte = 0; byte < 3; byte++) {

            for (int bit = 0; bit < 8; bit++) {

                bool on = row_data[byte] & (1 << (7 - bit));

                ssd1309_draw_pixel(
                    x + byte * 8 + bit,
                    y + row,
                    on
                );
            }
        }
    }
}

void ssd1309_draw_big_text(int x, int y, const char *text){
    while (*text) {
        ssd1309_draw_big_char(x, y, *text++);
        x += 24;
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
