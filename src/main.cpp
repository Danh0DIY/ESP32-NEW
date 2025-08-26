#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define HALL_SENSOR ADC1_CHANNEL_0 // Kênh ADC mặc định cho cảm biến Hall (có thể thay đổi)

void app_main(void) {
    // Khởi tạo UART (thay cho Serial.begin)
    printf("Kiểm tra cảm biến Hall - Bắt đầu!\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Đợi 1 giây

    // Cấu hình ADC cho cảm biến Hall
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_hall(HALL_SENSOR, ADC_ATTEN_DB_11); // Cấu hình kênh Hall

    while (1) {
        // Đọc giá trị từ cảm biến Hall
        int hallValue = hall_sensor_read(); // Sử dụng API của ESP-IDF

        // In giá trị ra UART
        printf("Giá trị cảm biến Hall: %d\n", hallValue);

        // Đợi 500ms
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}