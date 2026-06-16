#pragma once
#include <stdint.h>
#include <stdbool.h>

void ssd1309_draw_xbm(int x, int y, int w, int h, const uint8_t *bitmap);
void ssd1309_draw_pixel(int x, int y, bool on);
void ssd1309_draw_char(int x, int y, char c);
void ssd1309_draw_text(int x, int y, const char *text);
void ssd1309_draw_big_char(int x, int y, char c);
void ssd1309_draw_big_text(int x, int y, const char *text);
void progress_bar_fill(int start, int end, int lag_ms, int post_delay_ms);
