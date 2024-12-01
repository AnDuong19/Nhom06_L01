#include "oled.h"
#include "driver/i2c.h"

// I2C Configuration
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define OLED_ADDR 0x3C
// Cấu hình I2C

// Font 5x8 cho các số từ '0' đến '9' và dấu ':'
const uint8_t font5x8_digits[11][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
    {0x00, 0x36, 0x36, 0x00, 0x00}  // ':' (Dấu hai chấm)
};
const uint8_t font5x8[96][5] = {
    [33] = {0x7C, 0x12, 0x11, 0x12, 0x7C}, // 5x8'A'
    [34] = {0x7F, 0x49, 0x49, 0x49, 0x36}, // 5x8'B'
    [35] = {0x3E, 0x41, 0x41, 0x41, 0x22}, // 5x8'C'
    [36] = {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 5x8'D'
    [37] = {0x7F, 0x49, 0x49, 0x49, 0x41}, // 5x8'E'
    [38] = {0x7F, 0x09, 0x09, 0x01, 0x01}, // 5x8'F'
    [39] = {0x3E, 0x41, 0x41, 0x51, 0x73}, // 5x8'G'
    [40] = {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 5x8'H'
    [41] = {0x41, 0x7F, 0x41, 0x00, 0x00}, // 5x8'I'
    [42] = {0x02, 0x01, 0x41, 0x7F, 0x40}, // 5x8'J'
    [43] = {0x7F, 0x08, 0x14, 0x22, 0x41}, // 5x8'K'
    [44] = {0x7F, 0x80, 0x80, 0x80, 0x80}, // 5x8'L'
    [45] = {0x7F, 0x20, 0x10, 0x20, 0x7F}, // 5x8'M'
    [46] = {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 5x8'N'
    [47] = {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 5x8'O'
    [48] = {0x7F, 0x09, 0x09, 0x09, 0x06}, // 5x8'P'
    [49] = {0x3E, 0x41, 0x41, 0x43, 0x3F}, // 5x8'Q'
    [50] = {0x7F, 0x09, 0x19, 0x29, 0x46}, // 5x8'R'
    [51] = {0x26, 0x49, 0x49, 0x49, 0x32}, // 5x8'S'
    [52] = {0x01, 0x01, 0x7F, 0x01, 0x01}, // 5x8'T'
    [53] = {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 5x8'U'
    [54] = {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 5x8'V'
    [55] = {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 5x8'W'
    [56] = {0x63, 0x14, 0x08, 0x14, 0x63}, // 5x8'X'
    [57] = {0x07, 0x08, 0x70, 0x08, 0x07}, // 5x8'Y'
    [58] = {0x43, 0x45, 0x49, 0x51, 0x61}, // 5x8'Z'

};
// Khởi tạo I2Cx
void i2c_master_init() {
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &config));
    i2c_param_config(I2C_MASTER_NUM, &config);
    //i2c_driver_delete(I2C_NUM_0);
    i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
}

// Gửi lệnh điều khiển đến OLED
void oled_send_command(uint8_t cmd) {
    uint8_t buffer[2] = {0x00, cmd};
    i2c_master_write_to_device(I2C_MASTER_NUM, OLED_ADDR, buffer, 2, 1000 / portTICK_PERIOD_MS);
}

// Gửi dữ liệu đến OLED
void oled_send_data(const uint8_t *data, size_t length) {uint8_t buffer[129];
    buffer[0] = 0x40;  // Co/C = 0, D/C# = 1
    memcpy(&buffer[1], data, length);
    i2c_master_write_to_device(I2C_MASTER_NUM, OLED_ADDR, buffer, length + 1, 1000 / portTICK_PERIOD_MS);
}

// Khởi tạo OLEDy6
void oled_init() {
    oled_send_command(0xAE); // Display off
    oled_send_command(0xA8); // Set multiplex
    oled_send_command(0x3F); // 64 MUX
    oled_send_command(0xD3); // Display offset
    oled_send_command(0x00); // No offset
    oled_send_command(0x40); // Start line address
    oled_send_command(0xA1); // Segment re-map
    oled_send_command(0xC8); // COM output scan direction
    oled_send_command(0xDA); // COM pins hardware configuration
    oled_send_command(0x12);
    oled_send_command(0x81); // Contrast control
    oled_send_command(0x7F);
    oled_send_command(0xA4); // Entire display ON
    oled_send_command(0xA6); // Normal display
    oled_send_command(0xAF); // Display ON
}

// Xóa màn hình OLED
void oled_clear() {
    uint8_t buffer[1024] = {0};
    for (int i = 0; i < 8; i++) {
        oled_send_command(0xB0 + i); // Page address
        oled_send_command(0x00);     // Lower column start
        oled_send_command(0x10);     // Higher column start
        oled_send_data(buffer, 128); // Clear page
    }
}

// Vẽ một ký tự số hoặc dấu ':' lên OLED
void oled_draw_digit(uint8_t x, uint8_t y, char digit) {
    if (digit >= '0' && digit <= '9') {
        uint8_t index = digit - '0';
        oled_send_command(0xB0 + y);        // Page address
        oled_send_command(x & 0x0F);        // Lower column start
        oled_send_command(0x10 | (x >> 4)); // Higher column start
        oled_send_data(font5x8_digits[index], 5);
    } else if (digit == ':') {
        oled_send_command(0xB0 + y);
        oled_send_command(x & 0x0F);
        oled_send_command(0x10 | (x >> 4));
        oled_send_data(font5x8_digits[10], 5);  // Dấu ':'
    }
}

void oled_draw_char(uint8_t x, uint8_t y, char c, const uint8_t font[][5], uint8_t width) {
    if (c < 32 || c > 127) return; // Bỏ qua ký tự không hợp lệ
    uint8_t index = c - 32;
    oled_send_command(0xB0 + y);        // Page address
    oled_send_command(x & 0x0F);        // Lower column start
    oled_send_command(0x10 | (x >> 4)); // Higher column start
    oled_send_data(font[index], width); // Gửi dữ liệu của ký tự
}

void oled_draw_str(uint8_t x, uint8_t y, const char *str, const uint8_t font[][5], uint8_t width) {
    while (*str) {
        oled_draw_char(x, y, *str, font, width);
        x += width + 1; // Dịch sang phải (khoảng cách 1 pixel)
        str++;
    }
}
// Vẽ chuỗi thời gian "hh:mm"
void oled_draw_time(uint8_t x, uint8_t y, const char *time_str) {
    while (*time_str) {
        oled_draw_digit(x, y, *time_str);
        x += 6;  // Dịch sang phải (5 pixel ký tự + 1 pixel khoảng cách)
        time_str++;
    }
}

void draw_time(char *time){
    oled_clear();   
    oled_draw_time(50, 4, time);
}

void draw_verifying(){
    oled_clear();
    //Chuyển sang font 5x8
    int a = 40;
    oled_draw_str(a, 2, "V", font5x8, 5);
    oled_draw_str(a + 6, 2, "E", font5x8, 5);
    oled_draw_str(a + 12, 2, "R", font5x8, 5);
    oled_draw_str(a + 18, 2, "I", font5x8, 5);
    oled_draw_str(a + 24, 2, "F", font5x8, 5);
    oled_draw_str(a + 30, 2, "Y", font5x8, 5);
    oled_draw_str(a + 36, 2, "I", font5x8, 5);
    oled_draw_str(a + 42, 2, "N", font5x8, 5);
    oled_draw_str(a + 48, 2, "G", font5x8, 5); 
}

void draw_success(){
    oled_clear();
    int a = 42;
    oled_draw_str(a, 2, "V", font5x8, 5);
    oled_draw_str(a + 6, 2, "E", font5x8, 5);
    oled_draw_str(a + 12, 2, "R", font5x8, 5);
    oled_draw_str(a + 18, 2, "I", font5x8, 5);
    oled_draw_str(a + 24, 2, "F", font5x8, 5);
    oled_draw_str(a + 30, 2, "Y", font5x8, 5);
    a = 46;
    oled_draw_str(a, 3, "S", font5x8, 5);
    oled_draw_str(a + 6, 3, "U", font5x8, 5);
    oled_draw_str(a + 12, 3, "C", font5x8, 5);
    oled_draw_str(a + 18, 3, "C", font5x8, 5);
    oled_draw_str(a + 24, 3, "E", font5x8, 5);
    oled_draw_str(a + 30, 3, "S", font5x8, 5);
    oled_draw_str(a + 36, 3, "S", font5x8, 5);
}
void draw_fail(){
    oled_clear();
    int a = 42;
    oled_draw_str(a, 2, "V", font5x8, 5);
    oled_draw_str(a + 6, 2, "E", font5x8, 5);
    oled_draw_str(a + 12, 2, "R", font5x8, 5);
    oled_draw_str(a + 18, 2, "I", font5x8, 5);
    oled_draw_str(a + 24, 2, "F", font5x8, 5);
    oled_draw_str(a + 30, 2, "Y", font5x8, 5);
    a = 46;
    oled_draw_str(a, 3, "F", font5x8, 5);
    oled_draw_str(a + 6, 3, "A", font5x8, 5);
    oled_draw_str(a + 12, 3, "I", font5x8, 5);
    oled_draw_str(a + 18, 3, "L", font5x8, 5);
}
