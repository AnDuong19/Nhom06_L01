#ifndef OLED_H
#define OLED_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>


// Font Definitions
extern const uint8_t font5x8_digits[11][5];
extern const uint8_t font5x8[96][5];

// Function Prototypes
void i2c_master_init();
void oled_init();
void oled_send_command(uint8_t cmd);
void oled_send_data(const uint8_t *data, size_t length);
void oled_clear();
void oled_draw_digit(uint8_t x, uint8_t y, char digit);
void oled_draw_char(uint8_t x, uint8_t y, char c, const uint8_t font[][5], uint8_t width);
void oled_draw_str(uint8_t x, uint8_t y, const char *str, const uint8_t font[][5], uint8_t width);
void oled_draw_time(uint8_t x, uint8_t y, const char *time_str);
void draw_time(char *time);
void draw_verifying();
void draw_success();
void draw_fail();

#endif // OLED_DISPLAY_H
