#ifdef __cplusplus
extern "C" {
#endif

void ble_server_start(void);
void send_gesture(char letter);

#ifdef __cplusplus
}
#endif

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
#include "driver/touch_pad.h"

extern void ble_server_start(void);
extern void send_gesture(char letter);


//ADC channel assignments
#define FLEX_PIN_1 ADC1_CHANNEL_0   // GPIO36, Thumb
#define FLEX_PIN_2 ADC1_CHANNEL_3   // GPIO39, Index
#define FLEX_PIN_3 ADC1_CHANNEL_4   // GPIO32, Middle
#define FLEX_PIN_4 ADC1_CHANNEL_7   // GPIO35, Ring
#define FLEX_PIN_5 ADC1_CHANNEL_6   // GPIO34, Pinky
#define INDEX_TOUCH_PIN    TOUCH_PAD_NUM0 // GPIO4, Index touch sensor
#define MIDDLE_TOUCH_PIN   TOUCH_PAD_NUM7 // GPIO27, Middle touch sensor
#define THUMB_TOUCH_PIN    TOUCH_PAD_NUM8 // GPIO33, Thumb touch sensor
#define TOUCH_THRESHOLD    150 // Threshold for touch detection
#define NUM_TOUCH_INPUTS   3

#define BUFFER_SIZE 20 //Number of readings to calculate the running average
// Buffers for running averages
static float thumb_buffer[BUFFER_SIZE] = {0};
static float index_buffer[BUFFER_SIZE] = {0};
static float middle_buffer[BUFFER_SIZE] = {0};
static float ring_buffer[BUFFER_SIZE] = {0};
static float pinky_buffer[BUFFER_SIZE] = {0};
static int buffer_index = 0; // Circular buffer index

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
#define GAMMA 0.12499999999999997f //RBF kernel parameter




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
            //alpha_i = dual coefficients, K(x_i, x) = RBF kernel function, b = intercept
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



float calculate_running_average(float *buffer, int size) {
    float sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += buffer[i];
    }
    return sum / size;
}

float *read_flex_sensors() {
    static int buffer_count = 0;
    static float reading[NUM_FEATURES];
    int flexValue1 = adc1_get_raw(FLEX_PIN_1);
    int flexValue2 = adc1_get_raw(FLEX_PIN_2);
    int flexValue3 = adc1_get_raw(FLEX_PIN_3);
    int flexValue4 = adc1_get_raw(FLEX_PIN_4);
    int flexValue5 = adc1_get_raw(FLEX_PIN_5);
    
    uint16_t IndexTouch = 0, MiddleTouch = 0, ThumbTouch = 0;
    touch_pad_read(INDEX_TOUCH_PIN, &IndexTouch);
    touch_pad_read(MIDDLE_TOUCH_PIN, &MiddleTouch);
    touch_pad_read(THUMB_TOUCH_PIN, &ThumbTouch);



    //ADC to Voltage calculation
    float Thumb = flexValue1 * (3.3 / 4095.0);
    float Index = flexValue2 * (3.3 / 4095.0);
    float Middle = flexValue3 * (3.3 / 4095.0);
    float Ring = flexValue4 * (3.3 / 4095.0);
    float Pinky = flexValue5 * (3.3 / 4095.0);

    // Update buffers
    thumb_buffer[buffer_index] = Thumb;
    index_buffer[buffer_index] = Index;
    middle_buffer[buffer_index] = Middle;
    ring_buffer[buffer_index] = Ring;
    pinky_buffer[buffer_index] = Pinky;


    // printf("Raw ADC: Thumb=%d, Index=%d, Middle=%d, Ring=%d, Pinky=%d\n", flexValue1, flexValue2, flexValue3, flexValue4, flexValue5);

    // Update buffer index (circular buffer)
    buffer_index = (buffer_index + 1) % BUFFER_SIZE;

    buffer_count++;

    // Only calculate and return average every 20 samples
    if (buffer_count < BUFFER_SIZE) {
        return NULL;
    }

    // Calculate running averages
    reading[0] = calculate_running_average(thumb_buffer, BUFFER_SIZE);
    reading[1] = calculate_running_average(index_buffer, BUFFER_SIZE);
    reading[2] = calculate_running_average(middle_buffer, BUFFER_SIZE);
    reading[3] = calculate_running_average(ring_buffer, BUFFER_SIZE);
    reading[4] = calculate_running_average(pinky_buffer, BUFFER_SIZE);

    // Capacitive touch readings (inverted logic: lower = touched)
    reading[5] = (IndexTouch < TOUCH_THRESHOLD) ? 1.0 : 0.0; // Index touch sensor
    reading[6] = (MiddleTouch < TOUCH_THRESHOLD) ? 1.0 : 0.0; // Middle touch sensor
    reading[7] = (ThumbTouch < TOUCH_THRESHOLD) ? 1.0 : 0.0; // Thumb touch sensor


    // printf("K,%.3f,%.3f,%.3f,%.3f,%.3f,%.0f,%.0f,%.0f,\n", Thumb, Index, Middle, Ring, Pinky, reading[5], reading[6], reading[7]);
    printf("\nFlex Sensor Readings:\n");
    printf("Thumb Voltage = %.3f V\n", Thumb);
    printf("Index Voltage = %.3f V\n", Index);
    printf("Middle Voltage = %.3f V\n", Middle);
    printf("Ring Voltage = %.3f V\n", Ring);
    printf("Pinky Voltage = %.3f V\n", Pinky);
    printf("Touch Raw: Index=%d, Middle=%d, Thumb=%d\n", IndexTouch, MiddleTouch, ThumbTouch);
    printf("Touch Interpreted: I=%.0f M=%.0f T=%.0f\n", reading[5], reading[6], reading[7]);
    vTaskDelay(pdMS_TO_TICKS(500));
    return reading;
}



void app_main(){
    // Configure ADC for each channel
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(FLEX_PIN_1, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_2, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_3, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_4, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(FLEX_PIN_5, ADC_ATTEN_DB_12);

    //configure touch sensors
    touch_pad_init();
    touch_pad_config(INDEX_TOUCH_PIN, 0);
    touch_pad_config(MIDDLE_TOUCH_PIN, 0);
    touch_pad_config(THUMB_TOUCH_PIN, 0);

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
    ble_server_start();


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
    load_scalers();
    load_model_parameters();
    printf("System ready. Press space to read flex sensors and classify a gesture\n");
    printf("\nLetter,Thumb,Index,Middle,Ring,Pinky,IndexTouch,MiddleTouch,ThumbTouch\n");

    uint8_t keypress;
    while (1) {
        // Read UART input
        float *x= read_flex_sensors();

        if (x != NULL){
            scale_input(x);
            int gesture = predict(x);
            send_gesture('A' + gesture); //send gesture to BLE server
            printf("Predicted gesture: %c\n", 'A' + gesture);
            printf("Predicted gesture: %d\n", gesture); //for debugging
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}