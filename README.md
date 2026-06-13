# 📡 ESP32 Wireless Tally System

[![Firmware Version](https://img.shields.io/badge/Firmware-v1-blue.svg)](https://github.com/miguel-alegria14/tally-esp32-tallyarbiter/)
[![Compatible with](https://img.shields.io/badge/Compatible%20con-Tally%20Arbiter%203.x-orange.svg)](https://github.com/miguel-alegria14/tally-esp32-tallyarbiter/)
[![Platform](https://img.shields.io/badge/Platform-ESP32-green.svg)](https://espressif.com)

Ultra-low latency wireless Tally device based on the **LOLIN32 (ESP32)** hardware architecture. Specially engineered for demanding live broadcast environments, mitigating battery voltage drop bottlenecks through optimized RF and power management firmware.

* If you need to compile the code, check the [Compilation and Development Guide](COMPILATION.md).
* For camera operators, download the [User Manual](user_manual).

---

## 🚀 Key Features

* ⚡ **Anti-Lag Architecture:** Eliminates delays and freezing during rapid video switcher transitions by utilizing nested-loop-free linear processing.
* 🔋 **On-Demand Battery Monitoring:** The microcontroller halts constant analog pin sampling to eliminate antenna signal interference. It runs an automated background test every 5 minutes or checks instantly on-demand via the web dashboard.
* 📶 **Maximum Radio Power:** Hardware-forced disabling of the wireless modem sleep states (`WIFI_PS_NONE`). This maintains absolute network stability even when battery voltage drops from 4.2V down to 3.6V.
* 🌐 **Asynchronous AJAX Web Panel:** Network credential configuration and live device telemetry queries without needing page refreshes.

---

## 🚦 LED Indicator Diagnostics

The physical status of the hardware is actively displayed through three discrete channels:

| LED | State | Meaning |
| :--- | :--- | :--- |
| **🔴 RED (PROGRAM)** | Solid On | The camera is currently **LIVE / ON AIR**. |
| **🟢 GREEN (PREVIEW)** | Solid On | The camera is currently in **PREVIEW**. |
| **🔵 BLUE (STATUS/BAT)** | Fast Blink *(0.2s)* | The device is actively scanning or attempting to connect to the saved WiFi network. |
| | Ultra-Fast Blink *(0.1s)*| No known networks found. The local Configuration Access Point (AP) is active. |
| | Slow Blink *(2.0s)* | ⚠️ **Critical Alert:** Battery level is at **10% or lower**. Immediate charge required. |
| | Off | Connected to the server, normal operation, and optimal battery levels. |

---

## ⚙️ Initial Configuration and Web Control Panel

If the Tally device cannot find any stored network credentials in its non-volatile storage (NVS), it will automatically launch a rescue portal:

1. Connect your computer or phone to the open WiFi network: **`Config-Tally-AP`**.
2. Open your web browser and navigate to the IP address: **`192.168.4.1`**.
3. **On-Demand Battery Module:** To avoid interrupting time-critical network tasks, the power block displays the last static state. To read the live battery voltage, click the **"Ver nivel de batería (Dar para actualizar)"** button, which polls the analog pin instantly via an AJAX request.
4. Fill out your video server details, click save, and the Tally will automatically reboot into active production mode.

---

## 📁 Project Structure

```text
├── 📄 Tally_esp32_v1.ino      # Optimized source code (CPU clock locked at 240MHz & WiFi sleep disabled)
├── 📄 README.md              # Project presentation and quick hardware operation guide
├── 📄 COMPILATION.md         # Technical annex: IDE environment setup and v2.x dependencies
└── 📄 MANUAL_USUARIO.pdf     # Printed manual for camera operators and on-set stage technicians
