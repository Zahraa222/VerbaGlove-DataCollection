# VerbaGlove Embedded System

This repository contains the embedded firmware (C/C++) for **VerbaGlove**, a wearable assistive device that interprets American Sign Language (ASL) into text using machine learning and sends the result via Bluetooth to a mobile app.

> ⚠️ This repository is one part of a multi-repo system. It works in conjunction with:
- 🔗 [VerbaGlove-ML](https://github.com/Zahraa222/VerbaGlove-ML): SVM model training and CSV export.
- 📱 [VerbaGlove-app-android](https://github.com/F-Noori/VerbaGlove-app-android): BLE-connected Android app for displaying predictions and speech output.

---

## Overview

VerbaGlove is built on an ESP32 microcontroller and reads input from flex and capacitive touch sensors embedded in a glove. It predicts static ASL gestures using a One-vs-Rest SVM classifier and sends the results to a mobile device via Bluetooth Low Energy (BLE).

### Key Features
- Real-time gesture classification on ESP32
- Pre-trained ML model embedded as CSV files in LittleFS
- BLE communication to Android app
- Compatible with 21 ASL letters (excluding J & Z)

---

## ⚙️ How It Works

### Sensor Input
- **Flex Sensors**: Thumb, Index, Middle, Ring, Pinky (analog inputs)
- **Touch Sensors**: Detect touch between fingers (digital capacitive input)

### Preprocessing
- Running average over 20 samples per sensor
- Normalized using training set mean and standard deviation

### Prediction
- **Model Type:** One-vs-Rest SVM (RBF kernel)
- **Gamma:** 0.125
- **Classes:** A, B, C, D, E, F, G, H, I, K, L, O, P, Q, R, S, U, V, W, X, Y
- **Features:** 8 (5 flex sensor voltages, 3 touch states)

### 🧮 SVM Decision Function
Each binary classifier uses the following SVM decision function:
`f(x) = Σᵢ [ αᵢ * K(xᵢ, x) ] + b`
Where:
- `x` is the input feature vector
- `xᵢ` are the support vectors
- `αᵢ` are the dual coefficients
- `b` is the intercept
- `K(xᵢ, x)` is the RBF kernel:
`K(xᵢ, x) = exp(-γ * ||xᵢ - x||²)`
- `γ (gamma)` is set to `0.125`
  
The class with the highest `f(x)` score is selected as the predicted gesture.

### 📡 BLE Transmission
- BLE GATT server initialized with:
  - Device Name: `VerbaGlove`
  - Gesture Service UUID: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
  - Characteristic UUID: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`
- Sends predicted letter every ~200ms as a single-character notification

---

## 🔧 Installation & Setup

### ✅ Prerequisites
- ESP32 board (e.g. ESP-WROOM-32)
- ESP-IDF (v5.0 or later) installed
- Visual Studio Code or terminal with `idf.py`
- Model CSVs from [VerbaGlove-ML](https://github.com/Zahraa222/VerbaGlove-ML)

### 📂 Required CSV Files
The following files are **already included** in the repository under the `littlefs` folder. You just need to make sure they're properly built into the filesystem image and flashed to the ESP32.

**You only need to do this once:**

```bash
# 1. Create the FileSystem Image:
mklittlefs -c littlefs -b 4096 -p 256 -s 0x100000 build/littlefs.bin

# 2. Flash the filesystem partition (LittleFS) to your board
esptool.py --port COMx write_flash 0x210000 build/littlefs.bin
```
Files included:
- support_vectors_0.csv → support_vectors_20.csv
- dual_coef_0.csv → dual_coef_20.csv
- intercept_0.csv → intercept_20.csv 
- scaler_mean.csv 
- scaler_std.csv


### 🛠 Build & Flash
```bash
idf.py set-target esp32
idf.py menuconfig  # Enable LittleFS  and NimBLE if not already
idf.py build
idf.py flash monitor
```
### 📱 Mobile App
To display and vocalize gesture outputs, install and run the companion app:

VerbaGlove Android App

It connects via BLE, displays the predicted letter, and uses Google TTS to speak the output aloud.
