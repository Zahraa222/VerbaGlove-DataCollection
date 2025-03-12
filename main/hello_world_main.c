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

    //Voltage calculation
    float Thumb = flexValue1 * (3.3 / 4095.0);
    float Index = flexValue2 * (3.3 / 4095.0);
    float Middle = flexValue3 * (3.3 / 4095.0);
    float Ring = flexValue4 * (3.3 / 4095.0);
    float Pinky = flexValue5 * (3.3 / 4095.0);

    int count = 0;
    float sensor_readings[5][20] = {};
    float Thumb_Average = 0;
    float Index_Average = 0;
    float Middle_Average = 0;
    float Ring_Average = 0;
    float Pinky_Average = 0;

    if (flexValue1 && flexValue2 && flexValue3 && flexValue4 && flexValue5) {
        //Find sensor voltage Average value for every 20 readings
        if (count == 20) {
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < count; j++){
                    switch (i) {
                        case 0:
                            Thumb_Average += sensor_readings[i][j];
                            break;
                        case 1:
                            Index_Average += sensor_readings[i][j];
                            break;
                        case 2:
                            Middle_Average += sensor_readings[i][j];
                            break;
                        case 3:
                            Ring_Average += sensor_readings[i][j];
                            break;
                        case 4:
                            Pinky_Average += sensor_readings[i][j];
                            break;
                    }
                }
                Thumb_Average = Thumb_Average / count;
                Index_Average = Index_Average / count;
                Middle_Average = Middle_Average / count;
                Ring_Average = Ring_Average / count;
                Pinky_Average = Pinky_Average / count;
            }
            
        }

        for (int i = 0; i < 5; i++) {
            sensor_readings[i][count] = Thumb;
            sensor_readings[i][count] = Index;
            sensor_readings[i][count] = Middle;
            sensor_readings[i][count] = Ring;
            sensor_readings[i][count] = Pinky;

        }
        count++;

        printf("Flex Sensor Readings:\n");
        printf("Thumb Voltage = %.3f V\n", Thumb);
        printf("Index Voltage = %.3f V\n", Index);
        printf("Middle Voltage = %.3f V\n", Middle);
        printf("Ring Voltage = %.3f V\n", Ring);
        printf("Pinky Voltage = %.3f V\n", Pinky);
        printf("------------------------\n");
    }
    else {
        printf("Error reading flex sensors\n");
    }
    printf("\nFlex Sensor Readings:\n");
    printf("Thumb Voltage = %.3f V\n", Thumb);
    printf("Index Voltage = %.3f V\n", Index);
    printf("Middle Voltage = %.3f V\n", Middle);
    printf("Ring Voltage = %.3f V\n", Ring);
    printf("Pinky Voltage = %.3f V\n", Pinky);
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
