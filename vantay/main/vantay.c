#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "as608_driver.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "time.h"
#include "esp_sntp.h"
#include "connectwifi.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "oled.h"

#define TAG "ATTENDANCE_SYSTEM"
#define OLED_TAG "OLED_DISPLAY"

#define GOOGLE_SHEET_URL "https://script.google.com/macros/s/AKfycbw4HQ2KZkdjLaZnehB2p2fkW8hkoliwpv7bES5tl_1tUrOCP9p5SHh6K9-A5XreJvQ-tg/exec"

// GPIO Definitions
#define BUTTON_PIN GPIO_NUM_23   // Nút nhấn
#define TOUCH_PIN GPIO_NUM_19   // Cảm biến chạm (WAK từ AS608)

// Semaphore
SemaphoreHandle_t button_semaphore;
QueueHandle_t time_queue;

//static char current_time[64];    // Chuỗi lưu thời gian thực
bool fingerprint_verified = false; 

esp_err_t send_to_google_sheets(const char *data);
void time_sync_callback(struct timeval *tv);


// Trạng thái hệ thống
typedef enum {
    IDLE,   // 
    ENROLL,  // Chế độ lưu trữ vân tay
    VERIFYING
} fingerprint_state_t;

static fingerprint_state_t system_state = IDLE;

// Vị trí lưu vân tay hiện tại
static uint16_t fingerprint_position = 0;

// ISR: Xử lý nút nhấn
void IRAM_ATTR button_isr_handler(void *arg) {
    if (system_state == IDLE) {
        system_state = ENROLL;  // Chuyển sang chế độ lưu trữ
        xSemaphoreGiveFromISR(button_semaphore, NULL);
    }
}

// Cấu hình GPIO
static void gpio_config_init() {
    // Cấu hình nút nhấn
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE, // Kích hoạt ngắt khi nhấn
    };
    gpio_config(&button_config);

    // Cấu hình cảm biến chạm
    gpio_config_t touch_config = {
        .pin_bit_mask = (1ULL << TOUCH_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE, // Không cần ngắt
    };
    gpio_config(&touch_config);

    // Đăng ký ISR cho nút nhấn
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
}

// Task chính quản lý vân tay
void fingerprint_task(void *arg) {
    uint16_t matched_id = 0;
    uint16_t score = 0;
    char time_to_send[64];
    while (1) {
        switch (system_state) {
        case ENROLL:
            // Thực hiện lưu trữ vân tay
            fingerprint_verified = true;
            ESP_LOGI(TAG, "Starting fingerprint enrollment...");
            vTaskDelay(pdMS_TO_TICKS(3000));

            if (fingerprint_position > 175) {
                fingerprint_position = 0;
            }

            if (!as608_enroll_fingerprint(fingerprint_position)) {
                draw_fail();
                ESP_LOGE(TAG, "Failed to enroll fingerprint.");
            } else {
                draw_success();
                ESP_LOGI(TAG, "Fingerprint enrolled successfully at position %d!", fingerprint_position);
                fingerprint_position++;
            }

            // Trì hoãn để tránh kích hoạt chế độ xác thực ngay lập tức
            ESP_LOGI(TAG, "Enrollment complete. Please remove your finger.");
            vTaskDelay(pdMS_TO_TICKS(3000)); // Chờ người dùng nhấc tay

            // Quay lại chế độ chờ
            system_state = IDLE;
            fingerprint_verified = false;
            break;

        case IDLE:
            // Chế độ xác thực vân tay
            if (gpio_get_level(TOUCH_PIN) == 1) { // Kiểm tra tín hiệu cảm biến chạm
                system_state = VERIFYING;
            }
            break;

        case VERIFYING:
            fingerprint_verified = true;
            ESP_LOGI(TAG, "Detected touch. Verifying fingerprint...");
            draw_verifying();
            if (as608_verify_fingerprint(&matched_id, &score)) {
                draw_success();
                ESP_LOGI(TAG, "Access granted! Matched ID: %d, Score: %d", matched_id, score);
                if (xQueueReceive(time_queue, &time_to_send, pdMS_TO_TICKS(100)) == pdTRUE) {
                    ESP_LOGI(TAG, "Received time: %s", time_to_send);
                }
                    
                if (strcmp(time_to_send, "1970-01-01 07:00:03") != 0) {
                    char post_data[256];
                    snprintf(post_data, sizeof(post_data), "{\"ID\": \"%d\", \"Time\": \"%s\"}", matched_id, time_to_send);
                    ESP_LOGI(TAG, "Sending data: %s", post_data);
                    send_to_google_sheets(post_data);
                }
            } else {
                ESP_LOGW(TAG, "Access denied! Fingerprint not found.");
                draw_fail();
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
            fingerprint_verified = false;
            system_state = IDLE;
            vTaskDelay(pdMS_TO_TICKS(3000));
            break;
        }

        // Thêm độ trễ nhỏ để tránh task chiếm CPU
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Khởi tạo SNTP
void initialize_sntp(void)
{
    
    esp_sntp_setservername(0, "pool.ntp.org");  // SNTP server
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_set_time_sync_notification_cb(time_sync_callback);
    esp_sntp_init();
}

void time_sync_callback(struct timeval *tv)
{
    struct tm timeinfo;
    char strftime_buf[64];
    // Chuyển đổi thời gian
    localtime_r(&tv->tv_sec, &timeinfo);
    timeinfo.tm_hour += 7;
    // Định dạng thời gian
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "Time synchronized: %s", strftime_buf);
}

void time_sync_task(void *arg) {
    // Thiết lập múi giờ Việt Nam
    setenv("TZ", "UTC-7", 1);  // GMT+7
    tzset();                   // Áp dụng múi giờ
    char current_date[32]; // Biến lưu ngày (YYYY-MM-DD)
    char time[32]; // Biến lưu thời gian (HH:MM:SS)

    while (1) {
        // Lấy thời gian thực
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm timeinfo;
        localtime_r(&tv.tv_sec, &timeinfo);

        // Định dạng thời gian
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        strftime(current_date, sizeof(current_date), "%Y-%m-%d", &timeinfo);
        strftime(time, sizeof(time), "%H:%M:%S", &timeinfo);

        // Cập nhật thời gian thực an toàn qua Mutex
        if (xQueueSend(time_queue, &strftime_buf, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to send time to queue");
        }

        // Hiển thị thời gian lên OLED nếu không xác thực vân tay
        if (!fingerprint_verified) {
            //oled_display_time(current_time);
        ESP_LOGI(OLED_TAG, "Current date displayed on OLED: %s", current_date);
        ESP_LOGI(OLED_TAG, "Current time displayed on OLED: %s", time);
        draw_time(time);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}

// Hàm gửi dữ liệu đến Google Sheets
esp_err_t send_to_google_sheets(const char *post_data)
{
    esp_http_client_config_t config = {
        .url = GOOGLE_SHEET_URL,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    // Thiết lập body request
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Gửi HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Data sent to Google Sheets successfully");
    } else {
        ESP_LOGE(TAG, "Error sending data to Google Sheets: %s", esp_err_to_name(err));
    }

    // Giải phóng tài nguyên
    esp_http_client_cleanup(client);

    return err;
}


// Hàm chính
void app_main(void) {
    // Khởi tạo cảm biến AS608
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    if (!as608_init()) {
        ESP_LOGE(TAG, "Failed to initialize AS608.");
        return;
    }

    // Cấu hình GPIO
    gpio_config_init();
    i2c_master_init();
    oled_init();

    // Tạo Semaphore
    button_semaphore = xSemaphoreCreateBinary();
    if (button_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore.");
        return;
    }
    time_queue = xQueueCreate(5, sizeof(char[64]));
    if (time_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue.");
        return;
    }

    connect_wifi();
    initialize_sntp();

    // Tạo Task chính
    xTaskCreate(fingerprint_task, "Fingerprint Task", 4096, NULL, 5, NULL);
    xTaskCreate(time_sync_task, "time_sync_task", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Attendance system initialized.");
}
