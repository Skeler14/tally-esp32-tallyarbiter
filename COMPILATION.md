# 📡 Technical Annex: Compilation and Development Guide

Technical guide exclusively oriented for developers, firmware engineers, or technical support personnel in charge of compiling, debugging, and deploying the source code.

[![Document Version](https://img.shields.io/badge/Firmware-v1-blue.svg)](#)
[![Core Compatibility](https://img.shields.io/badge/esp32--core-2.0.17-orange.svg)](#)

## 1. Environment Preparation (Arduino IDE)
To prevent Heap memory allocation failures, syntax errors in the radio modem control, or miscalibrations in the analog ADC reading routines under battery power, it is mandatory to standardize the development environment under the stable **v2.x** branch from Espressif.

### Step-by-Step Core Installation:
1. Open the **Arduino IDE**.
2. Go to the menu **File > Preferences**.
3. In the **Additional Boards Manager URLs** section, paste the following JSON address:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
4. Navigate to **Tools > Board > Boards Manager**.
5. Search for **esp32** and strictly install version **2.0.17**.

> ⚠️ **COMPATIBILITY WARNING:** Do not update to core versions 3.x. Structural changes in Espressif's network APIs (`lwIP`) and the power HAL layer break Socket.IO thread persistence, reintroducing latency and lag issues during rapid switcher data bursts while running on battery.

---

## 2. Dependency Management
The firmware has been optimized by removing libraries that caused excessive dynamic memory (Heap) fragmentation. Install only the following dependencies via the **Library Manager** (`Tools > Manage Libraries...`):

### 📁 Required Dependencies Matrix

| # | Library | Required Version | Purpose and Technical Justification |
| :-: | :--- | :---: | :--- |
| **1** | **Arduino_JSON** *(Arduino)* | **v0.2.0** *(or higher)* | **Critical:** Official library for lightweight event block parsing. Prevents plain text buffer overhead during fast switcher data transitions. |
| **2** | **WebSockets** *(Markus Sattler)* | **v2.4.1** *(or higher)* | Low-latency, bidirectional transport layer coupled directly to the network stack. |
| **3** | **SocketIoClient** *(Ivan Conrado)* | **Latest Stable** | Socket.IO protocol implementation for synchronous capturing of `device_states` arrays. |

> ℹ️ **Optimization Note:** The external `WiFiManager` dependency and its heavy network buffers have been completely removed. The Captive Portal and background network scanning are now handled natively and asynchronously using the built-in `WiFi.h` and `WebServer.h` libraries.

---

## 3. Critical Hardware Configuration in the IDE
Before triggering the compilation and upload directive (`Upload`), verify that the Flash memory mapping and CPU clock speed match the following matrix exactly:

* **Board:** "WEMOS LOLIN32" (or "ESP32 Dev Module")
* **CPU Frequency:** **"240MHz (WiFi/BT)"** 🔴 *(Mandatory to prevent bottlenecks during fast camera tally transitions)*
* **Upload Speed:** "921600" (Reduce to 115200 if the physical COM port loses sync due to poor cable quality)
* **Flash Frequency:** "80MHz"
* **Flash Mode:** "QIO" (Quad I/O for maximum bus read speed)
* **Partition Scheme:** 🔴 "Minimal SPIFFS (Large APPS with OTA)"
* **Core Debug Level:** "None" (For a clean production environment free of Serial echo pollution)

---

## 4. Event Structure and Serial Diagnostics
When resetting the device via the physical button (`EN/RST`), verify the following clean initialization sequence in the Serial Monitor set to **115200 baud**:

```syslog
[WIFI] Initializing in Station (STA) mode...
[HARDWARE] CPU Frequency successfully configured to 240MHz.
[BATTERY] Initial voltage measurement completed.
[RADIO] Applying anti-lag directives: WiFi Sleep Disabled | PS Mode: NONE
[SOCKET] Attempting connection to Tally Arbiter Server...
[SOCKET] Connected - Emitting device handshake.
