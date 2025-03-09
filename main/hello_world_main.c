#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/uart.h"
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

//ADC channel assignments
#define FLEX_PIN_1 ADC1_CHANNEL_0   // GPIO36, Thumb
#define FLEX_PIN_2 ADC1_CHANNEL_3   // GPIO39, Index
#define FLEX_PIN_3 ADC1_CHANNEL_4   // GPIO32, Middle
#define FLEX_PIN_4 ADC1_CHANNEL_7   // GPIO35, Ring
#define FLEX_PIN_5 ADC1_CHANNEL_6   // GPIO34, Pinky

// UART Configuration
#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024

void read_flex_sensors() {
    int flexValue1 = adc1_get_raw(FLEX_PIN_1);
    int flexValue2 = adc1_get_raw(FLEX_PIN_2);
    int flexValue3 = adc1_get_raw(FLEX_PIN_3);
    int flexValue4 = adc1_get_raw(FLEX_PIN_4);
    int flexValue5 = adc1_get_raw(FLEX_PIN_5);

    float voltage1 = flexValue1 * (3.3 / 4095.0);
    float voltage2 = flexValue2 * (3.3 / 4095.0);
    float voltage3 = flexValue3 * (3.3 / 4095.0);
    float voltage4 = flexValue4 * (3.3 / 4095.0);
    float voltage5 = flexValue5 * (3.3 / 4095.0);

    printf("\nFlex Sensor Readings:\n");
    printf("Thumb Voltage = %.3f V\n", voltage1);
    printf("Index Voltage = %.3f V\n", voltage2);
    printf("Middle Voltage = %.3f V\n", voltage3);
    printf("Ring Voltage = %.3f V\n", voltage4);
    printf("Pinky Voltage = %.3f V\n", voltage5);
    printf("------------------------\n");
}

void app_main() {
    // Configure ADC for each channel
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(FLEX_PIN_1, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_2, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_3, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_4, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_5, ADC_ATTEN_DB_12);

    // Configure UART for reading input
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_driver_install(UART_NUM, BUF_SIZE, 0, 0, NULL, 0);

    uint8_t data;
    while (1) {
        // Read UART input
        int len = uart_read_bytes(UART_NUM, &data, 1, 10 / portTICK_PERIOD_MS);
        if (len > 0 && data == ' ') {
            read_flex_sensors();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
