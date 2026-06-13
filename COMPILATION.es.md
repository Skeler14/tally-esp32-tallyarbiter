# 📡 Anexo Técnico: Guía de Compilación y Desarrollo

Guía técnica orientada exclusivamente a desarrolladores, ingenieros de firmware o personal de soporte técnico encargado de compilar, depurar y realizar el despliegue del código fuente.

[![Document Version](https://img.shields.io/badge/Firmware-v1.1-blue.svg)](#)
[![Core Compatibility](https://img.shields.io/badge/esp32--core-2.0.17-orange.svg)](#)

## 1. Preparación del Entorno (Arduino IDE)
Para evitar fallos de asignación en memoria Heap, errores de sintaxis en el control del módem de radio o descalibraciones en las rutinas de lectura analógica del ADC bajo batería, es obligatorio homologar el entorno de desarrollo bajo la rama estable **v2.x** de Espressif.

### Paso a Paso para la Instalación del Core:
1. Abra el **Arduino IDE**.
2. Dirígete al menú **Archivo > Preferencias**.
3. En el apartado **Gestor de URLs Adicionales de Tarjetas**, pegue la siguiente dirección JSON:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
4. Navegue a **Herramientas > Placa > Gestor de Tarjetas**.
5. Busque **esp32** e instale estrictamente la versión **2.0.17**.

> ⚠️ **ADVERTENCIA DE COMPATIBILIDAD:** No actualice a las versiones del núcleo 3.x. Las modificaciones estructurales en las APIs de red de Espressif (`lwIP`) y la capa HAL de energía rompen la persistencia de los hilos de Socket.IO, reintroduciendo problemas de latencia o lag bajo ráfagas de datos en batería.

---

## 2. Gestión de Librerías Dependientes
El firmware ha sido optimizado eliminando librerías que abusaban de la fragmentación de memoria dinámica (Heap). Instale únicamente las siguientes dependencias mediante el **Gestor de Librerías** (`Herramientas > Administrar Bibliotecas...`):

### 📁 Cuadro de Dependencias Obligatorias

| # | Librería | Versión Requerida | Propósito y Justificación Técnica |
| :-: | :--- | :---: | :--- |
| **1** | **Arduino_JSON** *(Arduino)* | **v0.2.0** *(o superior)* | **Crítico:** Librería oficial para el parseo ligero en bloques de eventos. Evita la sobrecarga de buffers de texto planos en ráfagas rápidas de switcher. |
| **2** | **WebSockets** *(Markus Sattler)* | **v2.4.1** *(o superior)* | Capa de transporte bidireccional de baja latencia acoplada directamente al puerto de red. |
| **3** | **SocketIoClient** *(Ivan Conrado)* | **Última Estable** | Enlace del protocolo Socket.IO para la captura síncrona de los arreglos `device_states`. |

> ℹ️ **Nota de Optimización:** Se ha removido la dependencia externa `WiFiManager` y sus buffers pesados de red. El Portal Cautivo y el escaneo de redes circundantes ahora se procesan de forma nativa e de manera asíncrona usando las librerías integradas `WiFi.h` y `WebServer.h`.

---

## 3. Configuración Crítica del Hardware en el IDE
Antes de ejecutar la directiva de compilación y subida (`Upload`), verifique que el mapa de asignación de memoria Flash y el reloj del procesador coincidan exactamente con la siguiente matriz:

* **Placa (Board):** "WEMOS LOLIN32" (o "ESP32 Dev Module")
* **CPU Frequency:** **"240MHz (WiFi/BT)"** 🔴 *(Obligatorio para evitar cuello de botella en transiciones rápidas de cámara)*
* **Upload Speed:** "921600" (Reducir a 115200 si el puerto físico pierde sincronía por cable de baja calidad)
* **Flash Frequency:** "80MHz"
* **Flash Mode:** "QIO" (Quad I/O para máxima velocidad de lectura de bus)
* **Partition Scheme:** 🔴 "Minimal SPIFFS (Large APPS with OTA)"
* **Core Debug Level:** "Ninguno" (Para producción limpia y libre de ecos en el puerto serie)

---

## 4. Estructura de Eventos y Diagnóstico Serie
Al reiniciar el dispositivo mediante el botón físico (`EN/RST`), valide en el Monitor Serie configurado a **115200 baudios** la secuencia limpia de inicialización:

```syslog
[WIFI] Inicializando en modo Estación (STA)...
[HARDWARE] Frecuencia de CPU configurada a 240MHz de forma exitosa.
[BATTERY] Medición inicial de voltaje completada.
[RADIO] Aplicando directivas antimodulación: WiFi Sleep Desactivado | PS Mode: NONE
[SOCKET] Intentando enlace con Tally Arbiter Server...
[SOCKET] Conectado - Emitiendo handshake de dispositivo.
