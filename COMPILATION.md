# 📡 Technical Annex: Compilation and Development Guide

Technical guide intended exclusively for developers, firmware engineers, or technical support personnel responsible for compiling, debugging, and deploying the source code.

[![Document Version](https://img.shields.io/badge/Firmware-v1.0-blue.svg)](#) [![Core Compatibility](https://img.shields.io/badge/esp32--core-2.0.17-orange.svg)](#)

*🌐 [Leer esta guía en Español](COMPILATION.es.md)*

## 1. Environment Preparation (Arduino IDE)

To avoid syntax errors in low-level functions such as flushing the radio controller memory (`esp_bt_controller_mem_release()`) or native ADC calibration routines, it is mandatory to standardize the development environment under Espressif's stable **v2.x** branch.

### Core Installation Step-by-Step:

1. Open the **Arduino IDE**.
2. Navigate to the **File > Preferences** menu.
3. In the **Additional Boards Manager URLs** field, paste the following JSON address:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
4. Navigate to **Tools > Board > Boards Manager**.
5. Search for **esp32** and install strictly version **2.0.17**.

> ⚠️ **COMPATIBILITY WARNING:** Do not upgrade to core versions 3.x. Modifications in Espressif's internal network API alter the behavior of the `lwIP` network stack and break WebSocket stability in this Kernel.

---

## 2. Dependency Management

The v1.0 firmware implements an event-driven architecture based on Socket.IO, asynchronous JSON deserialization, and non-blocking web processing. Install the following dependencies using the **Library Manager** (`Tools > Manage Libraries...`):

### 📁 Required Dependencies Table

| # | Library | Required Version | Purpose and Technical Justification |
| :-: | :--- | :---: | :--- |
| **1** | **ArduinoJson** *(Benoit Blanchon)* | **v6.x** *(e.g., v6.21.5)* | **Critical:** Do not install the v7.x branch. The dynamic buffer `DynamicJsonDocument` syntax has been deprecated in the new version and will cause a fatal compilation error. |
| **2** | **WebSockets** *(Markus Sattler)* | **v2.4.1** *(or higher)* | Persistent, ultra-low-latency bidirectional transport layer running over the native TCP stack. |
| **3** | **SocketIoClient** *(Ivan Conrado)* | **Latest Stable** | Socket.IO protocol implementation compatible with Tally Arbiter server v3.x/v4.x engines. |
| **4** | **WiFiManager** *(tablatronix)* | **v2.0.16-rc** *(or higher)* | Asynchronous background Captive Portal orchestration using the non-blocking `wm.process()` method. |

---

## 3. Critical Hardware Configuration in the IDE

Before executing the compilation and upload directive (`Upload`), verify that the Flash memory mapping and physical transfer speeds exactly match the following matrix in the **Tools** menu:

* **Board:** "WEMOS LOLIN32" (or "ESP32 Dev Module")
* **Upload Speed:** "921600" (Reduce to 115200 if the COM port loses sync)
* **Flash Frequency:** "80MHz"
* **Flash Mode:** "QIO" (Quad I/O for maximum bus read speed)
* **Partition Scheme:** 🔴 "Minimal SPIFFS (Large APPS with OTA)"
* **Core Debug Level:** "None" (For clean production) or "Error" (For development environments)

### 🧠 Partition Map Analysis (Why Minimal SPIFFS?)

The compiled firmware binary has a net size of **1,047,577 bytes (~1.04 MB)**.

* Using the default partition scheme severely limits the maximum allocatable application space.
* By selecting **Minimal SPIFFS (Large APPS with OTA)**, the maximum executable space expands to **1.96 MB (1,966,080 bytes)**, leaving your binary at an optimal **53%** of disk utilization.
* This safety cushion of **~918 KB free** is a strict technical requirement for the **ArduinoOTA** library to download the new firmware in parallel into the secondary partition while the device continues to operate live on air safely.

---

## 4. Troubleshooting and Verification via Serial Monitor

Once the program is uploaded, open the IDE Serial Monitor configured strictly to **115200 baud** and press the physical reset button (`EN/RST`) on the board. You should validate the following Kernel initialization sequence:

```syslog
[POWER] Initializing system...
[HARDWARE] Bluetooth Low Energy stack flushed successfully.
[MEMORY] 51 KB dynamically injected to the execution Heap.
[NVS] Reading parameters from persistent non-volatile memory...
[NVS] Configuration loaded: Host: 192.168.1.150 | Port: 4455 | ID: CAM1
[WIFI] Initializing lwIP asynchronous communication stack.
[SYSTEM] Kernel v1.0 deployed successfully.
```
