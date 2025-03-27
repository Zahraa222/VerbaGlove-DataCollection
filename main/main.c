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
#include "math.h"
#include "esp_littlefs.h"

//ADC channel assignments
#define FLEX_PIN_1 ADC1_CHANNEL_0   // GPIO36, Thumb
#define FLEX_PIN_2 ADC1_CHANNEL_3   // GPIO39, Index
#define FLEX_PIN_3 ADC1_CHANNEL_4   // GPIO32, Middle
#define FLEX_PIN_4 ADC1_CHANNEL_7   // GPIO35, Ring
#define FLEX_PIN_5 ADC1_CHANNEL_6   // GPIO34, Pinky

// UART Configuration
#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024

void write_to_csv(float Thumb, float Index, float Middle, float Ring, float Pinky) {
    FILE* f = fopen("/spiffs/p.csv", "a");
    if (f == NULL) {
        printf("Failed to open file for writing\n");
        return;
    }
    fprintf(f, "p,%.3f,%.3f,%.3f,%.3f,%.3f\n", Thumb, Index, Middle, Ring, Pinky);
    fclose(f);
    //print content to manually add to csv file
    f=fopen("/spiffs/p.csv", "r");
    char line[128];  
    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }
    fclose(f);
}

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

    static int count = 0;
    static float sensor_readings[5][20] = {};
    static float Thumb_Average = 0;
    static float Index_Average = 0;
    static float Middle_Average = 0;
    static float Ring_Average = 0;
    static float Pinky_Average = 0;
    

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
            }
            Thumb_Average = Thumb_Average / count;
            Index_Average = Index_Average / count;
            Middle_Average = Middle_Average / count;
            Ring_Average = Ring_Average / count;
            Pinky_Average = Pinky_Average / count;
            
            // printf("Flex Sensor Readings:\n");
            // printf("Thumb Voltage = %.3f V\n", Thumb_Average);
            // printf("Index Voltage = %.3f V\n", Index_Average);
            // printf("Middle Voltage = %.3f V\n", Middle_Average);
            // printf("Ring Voltage = %.3f V\n", Ring_Average);
            // printf("Pinky Voltage = %.3f V\n", Pinky_Average);
            // printf("------------------------\n");

            // Reset averages and count after processing
            Thumb_Average = 0;
            Index_Average = 0;
            Middle_Average = 0;
            Ring_Average = 0;
            Pinky_Average = 0;
            count = 0;
        }
        else{
            sensor_readings[0][count] = Thumb;
            sensor_readings[1][count] = Index;
            sensor_readings[2][count] = Middle;
            sensor_readings[3][count] = Ring;
            sensor_readings[4][count] = Pinky;
            count++;
        }
    }
    else {
        printf("Error reading flex sensors\n");
    }

    // Write readings to CSV file
    //write_to_csv(Thumb, Index, Middle, Ring, Pinky);


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

    //Initialize littleFS
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);

    if (err != ESP_OK) {
        if (err == ESP_FAIL) {
            printf("Failed to mount or format filesystem\n");
        } else if (err == ESP_ERR_NOT_FOUND) {
            printf("Failed to find LittleFS partition\n");
        } else {
            printf("Failed to initialize LittleFS (%s)\n", esp_err_to_name(err));
        }
        return;
    }
    size_t total = 0, used = 0;
    err = esp_littlefs_info("littlefs", &total, &used);
    if (err != ESP_OK) {
        printf("Failed to get LittleFS partition information (%s)\n", esp_err_to_name(err));
    } else {
        printf("Partition size: total: %d, used: %d\n", total, used);
    }

    //Check to reading eligibility of flashed file
    FILE* f = fopen("/littlefs/scaler_std.csv", "r");
    if (f) {
        char line[100];
        while (fgets(line, sizeof(line), f)) {
            printf("Line: %s\n", line);
        }
        fclose(f);
    } else {
        printf("Failed to open scaler_std.csv\n");
    }

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
