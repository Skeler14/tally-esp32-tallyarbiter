# 📡 Sistema Tally Inalámbrico ESP32

[![Firmware Version](https://img.shields.io/badge/Firmware-v1.0-blue.svg)](https://github.com/miguel-alegria14/tally-esp32-tallyarbiter/)
[![Compatible con](https://img.shields.io/badge/Compatible%20con-Tally%20Arbiter%203.x-orange.svg)](https://github.com/miguel-alegria14/tally-esp32-tallyarbiter/)
[![Platform](https://img.shields.io/badge/Plataforma-ESP32-green.svg)](https://espressif.com)

Dispositivo Tally inalámbrico de baja latencia basado en el hardware **LOLIN32 (ESP32)**, diseñado para integrarse de forma nativa con **Tally Arbiter 3.x** mediante el protocolo Socket.IO sobre WebSockets. Ideal para producciones audiovisuales profesionales en vivo.

* Si deseas compilar el código, consulta la [Guía de Compilación y Desarrollo](COMPILATION.md).
* Para el operador de cámara, descarga el [Manual de Usuario](MANUAL_DE_USUARIO.pdf).

---

## 🚀 Características Principales

* ⚡ **Baja Latencia:** Respuesta inmediata a los cambios de estado en el mezclador de video.
* 🔋 **Eficiencia Energética:** Gestión crítica de energía con modos de bajo consumo (Deep Sleep) automáticos y apagado manual.
* 🌐 **Portal Cautivo Integrado:** Configuración inicial sencilla vía WiFi sin necesidad de reprogramar el chip.
* 🔄 **Reconexión Automática:** Algoritmo tolerante a fallos para recuperación ante pérdidas de señal o reinicios del servidor.
* ☁️ **Actualizaciones OTA:** Soporte para actualización inalámbrica de firmware a través de la red local.

---

## 🚦 Significado de los Indicadores LED

El estado del dispositivo se reporta visualmente a través de tres indicadores clave:

| LED | Estado | Significado |
| :--- | :--- | :--- |
| **🔴 ROJO (PROGRAM)** | Encendido Fijo | La cámara/dispositivo está actualmente **AL AIRE**. |
| | Parpadeo Rápido | El dispositivo no está conectado al servidor Tally Arbiter. |
| **🟢 VERDE (PREVIEW)** | Encendido Fijo | La cámara/dispositivo está en **PREVIO**. |
| **🔵 AZUL/BLANCO (BATERÍA/WIFI)** | Encendido Fijo | **Batería Baja** (Requiere carga o reemplazo). |
| | Parpadeando | Buscando o intentando conectar a la red WiFi. |
| | Apagado | Funcionamiento normal y batería óptima. |

---

## 🛠️ Modos de Operación Física (Botón BOOT)

El botón físico `BOOT` (GPIO 0) permite interactuar con el ciclo de vida del hardware:

1. **Apagado Manual (Deep Sleep):** Mantén presionado por **5 segundos**. Los LEDs se apagarán y el equipo entrará en modo de hibernación. Para despertar, presiona nuevamente el botón o reconecta la alimentación.
2. **Restablecimiento de Fábrica:** Mantén presionado por **10 segundos**. Se borrarán las redes WiFi guardadas, la IP del servidor y el Device ID. Al reiniciar, volverá a desplegar la red abierta `TallyConfig`.

---

## ⚙️ Primera Configuración

Si el dispositivo no detecta ninguna red conocida, actuará como punto de acceso:

1. Conéctate a la red WiFi generada por el dispositivo: **`TallyConfig`**.
2. Si el portal web no se abre automáticamente, ingresa en tu navegador la IP: **`192.168.4.1`**.
3. Rellena los campos del formulario:
    * **IP del servidor Tally Arbiter**
    * **Puerto** (Por defecto `4455`)
    * **Device ID** (Ej. `CAM1`, `PTZ1`)
4. Guarda la configuración. El equipo se reiniciará e iniciará su operación normal.

---

## 📁 Estructura del Proyecto

```text
├── 📄 Tally_esp32.ino        # Código fuente principal en C++ (Optimizado)
├── 📄 README.md              # Presentación general y manual de usuario rápido
├── 📄 COMPILATION.md         # Anexo técnico: Guía de compilación v1.0 para desarrolladores
└── 📄 MANUAL_USUARIO.pdf     # Manual de usuario en formato PDF para impresión/operadores
