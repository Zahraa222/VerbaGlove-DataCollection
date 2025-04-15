#include <string>
#include <NimBLEDevice.h>

#define DEVICE_NAME            "VerbaGlove"
//#define GESTURE_SERVICE_UUID   "180A"
//#define GESTURE_CHAR_UUID      "2A57"
#define GESTURE_SERVICE_UUID   "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define GESTURE_CHAR_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
        
#define GESTURE_MAX_LEN        20

static NimBLECharacteristic* pGestureCharacteristic = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        ESP_LOGI("BLE", "Client connected: %s", connInfo.getAddress().toString().c_str());
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        ESP_LOGW("BLE", "Client disconnected: %s, reason: %d",
                 connInfo.getAddress().toString().c_str(), reason);
        NimBLEDevice::startAdvertising();
    }
};

extern "C" void ble_server_start(void) {
    NimBLEDevice::init(DEVICE_NAME);

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(GESTURE_SERVICE_UUID);

    pGestureCharacteristic = pService->createCharacteristic(
        GESTURE_CHAR_UUID,
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
    );

    pGestureCharacteristic->setValue("A");

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(GESTURE_SERVICE_UUID);
    pAdvertising->start();

    ESP_LOGI("BLE", "BLE NimBLE server initialized and advertising");
}

extern "C" void send_gesture(char letter) {
    if (pGestureCharacteristic) {
        std::string gestureStr(1, letter);
        pGestureCharacteristic->setValue(gestureStr);
        pGestureCharacteristic->notify();
        ESP_LOGI("BLE", "Sent gesture: %c", letter);
    } else {
        ESP_LOGW("BLE", "Gesture characteristic not initialized");
    }
}