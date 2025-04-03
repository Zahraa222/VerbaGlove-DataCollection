#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"


#define DEVICE_NAME            "VerbaGlove"
#define GESTURE_SERVICE_UUID   0x00FF
#define GESTURE_CHAR_UUID      0xFF01
#define GESTURE_MAX_LEN        20
#define GATTS_APP_ID           0


static uint16_t service_handle = 0;
static esp_gatt_srvc_id_t service_id;
static uint16_t char_handle = 0;
static esp_bt_uuid_t char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = GESTURE_CHAR_UUID},
};
static uint8_t gesture_value[GESTURE_MAX_LEN] = "A";
static uint16_t conn_id = 0;
static esp_gatt_if_t global_gatts_if = 0;


static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .service_uuid_len = 2,
    .p_service_uuid = (uint8_t[]){GESTURE_SERVICE_UUID & 0xFF, (GESTURE_SERVICE_UUID >> 8) & 0xFF},
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT
};


static esp_attr_value_t gatts_char_val = {
    .attr_max_len = GESTURE_MAX_LEN,
    .attr_len = 1,
    .attr_value = gesture_value,
};




static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
};




void send_gesture(char letter) {
    if (global_gatts_if == 0 || conn_id == 0) {
        ESP_LOGW("BLE", "Not connected. Skipping send.");
        return;
    }
    gesture_value[0] = letter;
    esp_ble_gatts_set_attr_value(char_handle, 1, gesture_value);
    esp_ble_gatts_send_indicate(global_gatts_if, conn_id, char_handle, 1, gesture_value, false);
}


static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
        esp_ble_gap_start_advertising(&adv_params);
    }
}


static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            global_gatts_if = gatts_if;
            esp_ble_gap_set_device_name(DEVICE_NAME);
            esp_ble_gap_config_adv_data(&adv_data);


            service_id.is_primary = true;
            service_id.id.inst_id = 0x00;
            service_id.id.uuid.len = ESP_UUID_LEN_16;
            service_id.id.uuid.uuid.uuid16 = GESTURE_SERVICE_UUID;


            esp_ble_gatts_create_service(gatts_if, &service_id, 4);
            break;


        case ESP_GATTS_CREATE_EVT:
            service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(service_handle);
            esp_ble_gatts_add_char(service_handle, &char_uuid,
                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                &gatts_char_val, NULL);
            break;


        case ESP_GATTS_ADD_CHAR_EVT:
            char_handle = param->add_char.attr_handle;
            ESP_LOGI("BLE", "Service and characteristic created");
            esp_ble_gap_start_advertising(&adv_params);
            break;


        case ESP_GATTS_CONNECT_EVT:
            conn_id = param->connect.conn_id;
            global_gatts_if = gatts_if;
            ESP_LOGI("BLE", "Client connected");
            break;


        default:
            break;
    }
}


void ble_server_start() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());


    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(GATTS_APP_ID));
}

