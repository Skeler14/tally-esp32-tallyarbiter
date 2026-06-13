# 📡 Sistema Tally Inalámbrico ESP32

[![Firmware Version](https://img.shields.io/badge/Firmware-v1.1--Anti--Lag-blue.svg)](https://github.com/miguel-alegria14/tally-esp32-tallyarbiter/)
[![Compatible con](https://img.shields.io/badge/Compatible%20con-Tally%20Arbiter%203.x-orange.svg)](https://github.com/miguel-alegria14/tally-esp32-tallyarbiter/)
[![Platform](https://img.shields.io/badge/Plataforma-ESP32-green.svg)](https://espressif.com)

Dispositivo Tally inalámbrico de ultra-baja latencia basado en el hardware **LOLIN32 (ESP32)**. Diseñado específicamente para entornos de producción audiovisual exigentes, mitigando los cuellos de botella por caída de tensión en batería gracias a un firmware optimizado en la gestión de radiofrecuencia.

* Si deseas compilar el código, consulta la [Guía de Compilación y Desarrollo](COMPILATION.md).
* Para el operador de cámara, descarga el [Manual de Usuario](MANUAL_DE_USUARIO.pdf).

---

## 🚀 Características Principales

* ⚡ **Arquitectura Anti-Lag:** Eliminación de retrasos y congelamientos ante transiciones rápidas en el mezclador mediante procesamiento lineal libre de bucles aninados.
* 🔋 **Monitoreo Manual de Batería:** El chip detiene el muestreo constante sobre el pin analógico, eliminando interferencias en la antena. Realiza un test en segundo plano cada 5 minutos o bajo demanda desde el panel web.
* 📶 **Máxima Potencia de Radio:** Forzado de hardware para inhabilitar el modo reposo del módem inalámbrico (`WIFI_PS_NONE`). Mantiene la estabilidad de red incluso cuando el voltaje de la batería cae de 4.2V a 3.6V.
* 🌐 **Panel Web AJAX asíncrono:** Configuración de credenciales de red y consulta del estado del equipo de forma instantánea sin refrescar la página.

---

## 🚦 Significado de los Indicadores LED

El estado del equipo se visualiza de forma directa mediante tres canales físicos:

| LED | Estado | Significado |
| :--- | :--- | :--- |
| **🔴 ROJO (PROGRAM)** | Encendido Fijo | La cámara está actualmente **AL AIRE**. |
| **🟢 VERDE (PREVIEW)** | Encendido Fijo | La cámara está en **PREVIO**. |
| **🔵 AZUL (STATUS/BAT)** | Parpadeo Rápido *(0.2s)* | El dispositivo está buscando o intentando conectarse a la red WiFi configurada. |
| | Parpadeo Muy Rápido *(0.1s)*| El Tally no halló redes conocidas y activó el Punto de Acceso Local de Configuración. |
| | Parpadeo Lento *(2.0s)* | ⚠️ **Alerta Crítica:** Nivel de batería al **10% o inferior**. Requiere carga inmediata. |
| | Apagado | Conectado al servidor, operando con normalidad y nivel de batería óptimo. |

---

## ⚙️ Primera Configuración y Panel de Control Web

Si el Tally no se conecta a ninguna red almacenada en su memoria persistente (NVS), desplegará su propio portal de rescate:

1. Conéctate a la red WiFi abierta generada por la placa: **`Config-Tally-AP`**.
2. Abre tu navegador web e ingresa a la dirección IP: **`192.168.4.1`**.
3. **Módulo de Batería Bajo Demanda:** Para no interrumpir los procesos de red del microcontrolador, el bloque de energía mostrará de forma fija el último estado estático. Para conocer el voltaje real de operación en vivo, pulsa el botón **"Ver nivel de batería (Dar para actualizar)"**, el cual leerá el pin analógico mediante una petición AJAX instantánea.
4. Completa los parámetros del servidor de video, guarda los cambios y el Tally se reiniciará automáticamente para entrar en operación aérea.

---

## 📁 Estructura del Proyecto

```text
├── 📄 Tally_esp32_v1.ino      # Código fuente optimizado (Frecuencia de CPU fija a 240MHz y WiFi activo)
├── 📄 README.md              # Presentación general y manual de operación rápida de hardware
├── 📄 COMPILATION.md         # Anexo técnico: Guía de entorno de desarrollo y dependencias v2.x
└── 📄 MANUAL_USUARIO.pdf     # Manual físico para operadores de cámara y personal técnico en set
