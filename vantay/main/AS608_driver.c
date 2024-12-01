#include "as608_driver.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define TAG "AS608_DRIVER"

#define AS608_UART_NUM UART_NUM_1       // Sử dụng UART1
#define AS608_TX_PIN GPIO_NUM_17        // Chân TX của ESP32 nối với RX của AS608
#define AS608_RX_PIN GPIO_NUM_16        // Chân RX của ESP32 nối với TX của AS608
#define AS608_BAUD_RATE 57600           // Baud rate mặc định của AS608
#define AS608_UART_BUF_SIZE 1024        // Kích thước buffer UART

// Lệnh "Verify Password"
static const uint8_t verify_password_cmd[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07,
    0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B
};

// Lệnh "Generate Image"
static const uint8_t gen_image_cmd[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03,
    0x01, 0x00, 0x05
};

// Lệnh "Register Model"
static const uint8_t reg_model_cmd[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03,
    0x05, 0x00, 0x09
};

// Lệnh "Generate Character" với BufferID = 1 (CharBuffer1)
static const uint8_t gen_char_cmd_buf1[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,  // Header và địa chỉ mặc định
    0x01,                                // Package identifier (Command packet)
    0x00, 0x04,                          // Độ dài gói (4 bytes: Instruction + BufferID + Checksum)
    0x02,                                // Mã lệnh: Generate Character
    0x01,                                // BufferID: CharBuffer1
    0x00, 0x08                           // Checksum: 0x02 + 0x01 = 0x08
};

// Lệnh "Generate Character" với BufferID = 2 (CharBuffer2)
static const uint8_t gen_char_cmd_buf2[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,  // Header và địa chỉ mặc định
    0x01,                                // Package identifier (Command packet)
    0x00, 0x04,                          // Độ dài gói (4 bytes: Instruction + BufferID + Checksum)
    0x02,                                // Mã lệnh: Generate Character
    0x02,                                // BufferID: CharBuffer2
    0x00, 0x09                           // Checksum: 0x02 + 0x02 = 0x09
};

// Lệnh "Search Fingerprint" (tìm kiếm trong cơ sở dữ liệu)
static const uint8_t search_fingerprint_cmd[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, // Header và địa chỉ mặc định
    0x01,                              // Package identifier (Command packet)
    0x00, 0x08,                        // Độ dài gói (8 bytes: Instruction + BufferID + StartPage + PageNum + Checksum)
    0x04,                              // Mã lệnh: Search
    0x01,                              // BufferID: CharBuffer1
    0x00, 0x00,                        // StartPage: bắt đầu từ trang 0
    0x00, 0xAF,                        // PageNum: 175 trang (toàn bộ cơ sở dữ liệu)
    0x00, 0xBD                         // Checksum: 0x04 + 0x01 + 0x00 + 0x00 + 0x00 + 0xAF = 0xBC
};

// Cấu hình UART
static void uart_init() {
    const uart_config_t uart_config = {
        .baud_rate = AS608_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    uart_driver_install(AS608_UART_NUM, AS608_UART_BUF_SIZE, 0, 0, NULL, 0);
    uart_param_config(AS608_UART_NUM, &uart_config);
    uart_set_pin(AS608_UART_NUM, AS608_TX_PIN, AS608_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART Initialized.");
}

// Gửi lệnh đến cảm biến
static bool as608_send_command(const uint8_t *command, size_t length) {
    int written = uart_write_bytes(AS608_UART_NUM, (const char *)command, length);
    if (written != length) {
        ESP_LOGE(TAG, "Failed to send command to AS608");
        return false;
    }
    return true;
}

// Nhận phản hồi từ cảm biến
static bool as608_receive_response(uint8_t *response, size_t length) {
    int read = uart_read_bytes(AS608_UART_NUM, response, length, pdMS_TO_TICKS(5000));
    if (read > 0 && read < length) {
        ESP_LOGE(TAG, "Partial response received: %d/%d bytes", read, length);
        return false;
    } else if (read == 0) {
        ESP_LOGE(TAG, "No response received from AS608.");
        return false;
    }
    //if (read != length) {
      //  ESP_LOGE(TAG, "Failed to read response from AS608");
        //return false;
    //}
    return true;
}

// Xác thực mật khẩu
static bool send_verify_password() {
    uint8_t response[12];
    if (!as608_send_command(verify_password_cmd, sizeof(verify_password_cmd))) {
        return false;
    }
    if (!as608_receive_response(response, sizeof(response))) {
        return false;
    }
    if (response[9] != 0x00) {
        ESP_LOGE(TAG, "AS608 initialization failed: Error code 0x%02X", response[9]);
        return false;
    }
    ESP_LOGI(TAG, "Verify Password successful");
    return true;
}

// Tạo ảnh vân tay
static bool as608_generate_image() {
    uint8_t response[12];
    if (!as608_send_command(gen_image_cmd, sizeof(gen_image_cmd))) {
        return false;
    }
    //vTaskDelay(pdMS_TO_TICKS(500));
    if (!as608_receive_response(response, sizeof(response))) {
        return false;
    }
    if (response[9] != 0x00) {
        ESP_LOGE(TAG, "Generate image failed: Error code 0x%02X", response[9]);
        return false;
    }
    ESP_LOGI(TAG, "Generate image successful");
    return true;
}

// Hàm tạo đặc điểm từ ảnh vân tay
static bool as608_generate_character(uint8_t buffer_id) {
    uint8_t response[12];
    

    // Chọn lệnh dựa trên BufferID
    const uint8_t *command = (buffer_id == 1) ? gen_char_cmd_buf1 : gen_char_cmd_buf2;
    size_t command_len = (buffer_id == 1) ? sizeof(gen_char_cmd_buf1) : sizeof(gen_char_cmd_buf2);

    // Gửi lệnh Generate Character
    if (!as608_send_command(command, command_len)) { // Lệnh có độ dài 9 bytes
        
        return false;
    }

    // Nhận phản hồi từ cảm biến
    if (!as608_receive_response(response, sizeof(response))) {
        return false;
    }

    // Kiểm tra mã phản hồi
    if (response[9] != 0x00) {
        ESP_LOGE(TAG, "Generate Character failed for BufferID %d: Error code 0x%02X", buffer_id, response[9]);
        return false;
    }

    ESP_LOGI(TAG, "Generate Character successful for BufferID %d", buffer_id);
    return true;
}

// Tạo template từ ảnh vân tay
static bool as608_register_model() {
    uint8_t response[12];
    int retries = 3;  // Thử lại tối đa 3 lần
    while (retries-- > 0) {
        if (!as608_send_command(reg_model_cmd, sizeof(reg_model_cmd))) {
            ESP_LOGE(TAG, "Failed to send register model command");
            return false;
        }

        if (!as608_receive_response(response, sizeof(response))) {
            ESP_LOGE(TAG, "Failed to receive response for register model");
            return false;
        }

        if (response[9] == 0x00) {
            ESP_LOGI(TAG, "Register model successful");
            return true;
        }

        ESP_LOGW(TAG, "Register model failed, retrying... Error code 0x%02X", response[9]);
        vTaskDelay(pdMS_TO_TICKS(500));  // Chờ 500ms trước khi thử lại
    }
    ESP_LOGE(TAG, "Register model failed after retries");
    return false;
}

// Khởi tạo cảm biến AS608
bool as608_init() {
    uart_init();
    return send_verify_password();
}

// Đăng ký dấu vân tay
bool as608_enroll_fingerprint(uint16_t storage_position) {
    uint8_t store_cmd[] = {
        0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x06,
        0x06, 0x02, (uint8_t)(storage_position >> 8), (uint8_t)(storage_position & 0xFF), 0x00, 0x00
    };

    uint16_t checksum = 0x01 + 0x06 + 0x06 + 0x02 + (storage_position >> 8) + (storage_position & 0xFF);
    store_cmd[13] = (uint8_t)(checksum >> 8);
    store_cmd[14] = (uint8_t)(checksum & 0xFF);

    uint8_t response[12];

    // Yêu cầu người dùng đặt ngón tay lần đầu
    ESP_LOGI(TAG, "Please place your finger on the sensor.");
    vTaskDelay(pdMS_TO_TICKS(3000)); // Chờ 3 giây để người dùng đặt ngón tay

    if (!as608_generate_image()) {
        ESP_LOGE(TAG, "Failed to generate first image.");
        return false;
    }

    //vTaskDelay(pdMS_TO_TICKS(30));
    if (!as608_generate_character(1)) {
    ESP_LOGE(TAG, "Failed to generate character for first image.");
    return false;
}

    // Yêu cầu người dùng nhấc ngón tay
    ESP_LOGI(TAG, "Please remove your finger.");
    vTaskDelay(pdMS_TO_TICKS(3000)); // Chờ 3 giây để người dùng nhấc ngón tay

    // Yêu cầu người dùng đặt ngón tay lần hai
    ESP_LOGI(TAG, "Please place your finger again.");
    vTaskDelay(pdMS_TO_TICKS(3000)); // Chờ 3 giây để người dùng đặt ngón tay

    if (!as608_generate_image()) {
        ESP_LOGE(TAG, "Failed to generate second image.");
        return false;
    }
    //vTaskDelay(pdMS_TO_TICKS(30));
    if (!as608_generate_character(2)) {
    ESP_LOGE(TAG, "Failed to generate character for first image.");
    return false;
}
    // Tạo template từ ảnh
    if (!as608_register_model()) {
        ESP_LOGE(TAG, "Failed to create fingerprint template.");
        return false;
    }
    ESP_LOGI(TAG, "Storing fingerprint at position %d", storage_position);
    if (!as608_send_command(store_cmd, sizeof(store_cmd))) {
        return false;
    }
    if (!as608_receive_response(response, sizeof(response))) {
        return false;
    }
    if (response[9] != 0x00) {
        ESP_LOGE(TAG, "Store template failed: Error code 0x%02X", response[9]);
        return false;
    }
    ESP_LOGI(TAG, "Fingerprint stored successfully at position %d", storage_position);
    return true;
}

bool as608_verify_fingerprint(uint16_t *matched_id, uint16_t *score) {
    uint8_t response[16]; // Phản hồi từ module

    // Lấy hình ảnh vân tay
    ESP_LOGI(TAG, "Place your finger on the sensor.");
    vTaskDelay(pdMS_TO_TICKS(3000));
    if (!as608_generate_image()) {
        ESP_LOGE(TAG, "Failed to capture fingerprint image.");
        return false;
    }

    // Tạo đặc điểm từ hình ảnh
    if (!as608_generate_character(1)) {
        ESP_LOGE(TAG, "Failed to generate fingerprint character.");
        return false;
    }

    // Gửi lệnh tìm kiếm
    if (!as608_send_command(search_fingerprint_cmd, sizeof(search_fingerprint_cmd))) {
        ESP_LOGE(TAG, "Failed to send search fingerprint command.");
        return false;
    }

    // Nhận phản hồi
    if (!as608_receive_response(response, sizeof(response))) {
        ESP_LOGE(TAG, "Failed to receive response from search fingerprint command.");
        return false;
    }

    // Kiểm tra mã phản hồi
    if (response[9] != 0x00) {
        ESP_LOGE(TAG, "Fingerprint not found. Error code: 0x%02X", response[9]);
        return false;
    }

    // Đọc ID và điểm khớp
    *matched_id = (response[10] << 8) | response[11]; // Byte 10-11 chứa FingerID
    *score = (response[12] << 8) | response[13];     // Byte 12-13 chứa độ khớp (Score)

    ESP_LOGI(TAG, "Fingerprint matched! ID: %d, Score: %d", *matched_id, *score);
    return true;
}
