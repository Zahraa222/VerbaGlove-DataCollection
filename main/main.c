#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
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

extern void ble_server_start(void);
extern void send_gesture(char letter);

//ADC channel assignments
#define FLEX_PIN_1 ADC1_CHANNEL_0   // GPIO36, Thumb
#define FLEX_PIN_2 ADC1_CHANNEL_3   // GPIO39, Index
#define FLEX_PIN_3 ADC1_CHANNEL_4   // GPIO32, Middle
#define FLEX_PIN_4 ADC1_CHANNEL_7   // GPIO35, Ring
#define FLEX_PIN_5 ADC1_CHANNEL_6   // GPIO34, Pinky
#define INDEX_TOUCH_PIN    4 // GPIO4, Index touch sensor
#define MIDDLE_TOUCH_PIN   27 // GPIO27, Middle touch sensor
#define THUMB_TOUCH_PIN    33 // GPIO33, Thumb touch sensor
#define TOUCH_THRESHOLD    25 // Threshold for touch detection
#define NUM_TOUCH_INPUTS 3

// UART Configuration
#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024

//Machine Learning Model constants
#define NUM_MODELS 9 //Number of letters
#define NUM_FEATURES (5 + NUM_TOUCH_INPUTS) //Number of inputs
#define NUM_SUPPORT_VECTORS 33 //max number of support vectors across all models
float scaler_mean[NUM_FEATURES]; //mean  value per feature from training
float scaler_std[NUM_FEATURES]; // Standard deviation per feature from training


//Model parameters
float support_vectors[NUM_MODELS][NUM_SUPPORT_VECTORS][NUM_FEATURES];
float dual_coef[NUM_MODELS][NUM_SUPPORT_VECTORS]; //dual coefficients (alphas) for each support vector
float intercept[NUM_MODELS];
int num_sv[NUM_MODELS]; //number of support vectors for each model
#define GAMMA 0.2f //RBF kernel parameter




//load a row from csv file and fill an array
void load_csv_row(const char *path, float *arr, int len) {
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("Failed to open %s\n", path);
        return;
    }
    for (int i = 0; i < len; i++) {
        fscanf(f, "%f,", &arr[i]);
    }
    fclose(f);
}


void load_model_parameters() {
    char path[100];


    for (int i = 0; i < NUM_MODELS; i++) {
        // Load support vectors for model i
        snprintf(path, sizeof(path), "/littlefs/support_vectors_%d.csv", i);
        FILE* f_sv = fopen(path, "r");
        if (!f_sv) {
            printf("Failed to open %s\n", path);
            continue;
        }


        int sv_count = 0;
        while (!feof(f_sv) && sv_count < NUM_SUPPORT_VECTORS) {
            for (int j = 0; j < NUM_FEATURES; j++) {
                fscanf(f_sv, "%f,", &support_vectors[i][sv_count][j]);
            }
            sv_count++;
        }
        fclose(f_sv);
        num_sv[i] = sv_count;


        // Load dual coefficients for model i
        snprintf(path, sizeof(path), "/littlefs/dual_coef_%d.csv", i);
        FILE* f_coef = fopen(path, "r");
        if (!f_coef) {
            printf("Failed to open %s\n", path);
            continue;
        }
        for (int j = 0; j < sv_count; j++) {
            fscanf(f_coef, "%f,", &dual_coef[i][j]);
        }
        fclose(f_coef);
       
        // Load intercepts
        snprintf(path, sizeof(path), "/littlefs/intercept_%d.csv", i);
        FILE* f_int = fopen(path, "r");
        if (f_int) {
            fscanf(f_int, "%f,", &intercept[i]);
            fclose(f_int);
        } else {
            printf("Failed to open %s\n", path);
        }
    }
}


//TODO: maybe extract this manually rather than using the csv file?? saves memory on the ESP32
void load_scalers(){
    load_csv_row("/littlefs/scaler_mean.csv", scaler_mean, NUM_FEATURES);
    load_csv_row("/littlefs/scaler_std.csv", scaler_std, NUM_FEATURES);
}


//normalize input features(data)
//Equation: x' = (x - mean) / std, x=input, x'=normalized input
void scale_input(float *input){
    for(int i = 0; i < NUM_FEATURES; i++){
        input[i] = (input[i] - scaler_mean[i]) / scaler_std[i];
    }
}


//RBF kernel function
float rbf_kernel(float *x1, float *x2, int len, float gamma){
    float euclidean_distance = 0;
    for (int i = 0; i < len; i++){
        float diff = x1[i] - x2[i];
        euclidean_distance += diff * diff;
    }
    //RBF kernel equation: K(x1, x2) = exp(-gamma * ||x1 - x2||^2)
    return exp(-gamma * euclidean_distance);
}


//CLASSIFICATION: one-vs-rest strategy
//Returns index of class with the highest decision score
int predict(float *x){
    float max_score = -INFINITY;
    int best_class = -1;
    //iterate over each binary classifier
    for (int i = 0; i < NUM_MODELS; i++) {
        float decision = 0.0;
        for (int j = 0; j < num_sv[i]; j++) {
            //RBF SVM decision function: f(x) = sum(alpha_i * K(x_i, x)) + b
            //alpha_i = duall coreffiecients, K(x_i, x) = RBF kernel function, b = intercept
            decision += dual_coef[i][j] * rbf_kernel(support_vectors[i][j], x, NUM_FEATURES, GAMMA);
        }
        decision += intercept[i];
        if (decision > max_score) {
            max_score = decision;
            best_class = i;
        }
    }
    return best_class;
}


void read_flex_sensors() {
    static float reading[NUM_FEATURES];
    int flexValue1 = adc1_get_raw(FLEX_PIN_1);
    int flexValue2 = adc1_get_raw(FLEX_PIN_2);
    int flexValue3 = adc1_get_raw(FLEX_PIN_3);
    int flexValue4 = adc1_get_raw(FLEX_PIN_4);
    int flexValue5 = adc1_get_raw(FLEX_PIN_5);

    int touchValue1 = adc1_get_raw(INDEX_TOUCH_PIN);
    int touchValue2 = adc1_get_raw(MIDDLE_TOUCH_PIN);
    int touchValue3 = adc1_get_raw(THUMB_TOUCH_PIN);


    //ADC to Voltage calculation
    float Thumb = flexValue1 * (3.3 / 4095.0);
    float Index = flexValue2 * (3.3 / 4095.0);
    float Middle = flexValue3 * (3.3 / 4095.0);
    float Ring = flexValue4 * (3.3 / 4095.0);
    float Pinky = flexValue5 * (3.3 / 4095.0);
    float IndexTouch = touchValue1 * (3.3 / 4095.0);
    float MiddleTouch = touchValue2 * (3.3 / 4095.0);
    float ThumbTouch = touchValue3 * (3.3 / 4095.0);
    
    reading[0] = Thumb;
    reading[1] = Index;
    reading[2] = Middle;
    reading[3] = Ring;
    reading[4] = Pinky;

    // Capacitive touch readings (inverted logic: lower = touched)
    reading[5] = (IndexTouch < TOUCH_THRESHOLD) ? 1.0 : 0.0; // Index touch sensor
    reading[6] = (MiddleTouch < TOUCH_THRESHOLD) ? 1.0 : 0.0; // Middle touch sensor
    reading[7] = (ThumbTouch < TOUCH_THRESHOLD) ? 1.0 : 0.0; // Thumb touch sensor



    printf("\nFlex Sensor Readings:\n");
    printf("Thumb Voltage = %.3f V\n", Thumb);
    printf("Index Voltage = %.3f V\n", Index);
    printf("Middle Voltage = %.3f V\n", Middle);
    printf("Ring Voltage = %.3f V\n", Ring);
    printf("Pinky Voltage = %.3f V\n", Pinky);
    printf("Index Touch Voltage = %.3f V, Touched? %f\n", IndexTouch, reading[5]);
    printf("Middle Touch Voltage = %.3f V, Touched? %f\n", MiddleTouch, reading[6]);
    printf("Thumb Touch Voltage = %.3f V, Touched? %f\n", ThumbTouch, reading[7]);
    printf("------------------------\n\n\n");

    // Debug
    printf("Flex: T=%.2f I=%.2f M=%.2f R=%.2f P=%.2f | Touch: I=%.0f M=%.0f T=%.0f\n",
           reading[0], reading[1], reading[2], reading[3], reading[4],
           reading[5], reading[6], reading[7]);


    //return reading;
}


void app_main(){
    // Configure ADC for each channel
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(FLEX_PIN_1, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_2, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_3, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_4, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_5, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(INDEX_TOUCH_PIN, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(MIDDLE_TOUCH_PIN, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(THUMB_TOUCH_PIN, ADC_ATTEN_DB_12);


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


    // Start BLE server
    //ble_server_start();


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


    //load parameters
    //load_scalers();
    //load_model_parameters();
    //printf("System ready. Press space to read flex sensors and classify a gesture\n");


    uint8_t keypress;
    while (1) {
        // Read UART input
        int len = uart_read_bytes(UART_NUM, &keypress, 1, 10 / portTICK_PERIOD_MS);
        if (len > 0 && keypress == ' ') {
            read_flex_sensors();
            //float *x= read_flex_sensors();
            //scale_input(x);
            //int gesture = predict(x);
            //send_gesture('A' + gesture); //send gesture to BLE server
            //printf("Predicted gesture: %c\n", 'A' + gesture);
            //printf("Predicted gesture: %d\n", gesture); //for debugging
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}