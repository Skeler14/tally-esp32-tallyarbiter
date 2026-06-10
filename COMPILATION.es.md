# 📡 Anexo Técnico: Guía de Compilación y Desarrollo

Guía técnica orientada exclusivamente a desarrolladores, ingenieros de firmware o personal de soporte técnico encargado de compilar, depurar y realizar el despliegue del código fuente.

[![Document Version](https://img.shields.io/badge/Firmware-v1.0-blue.svg)](#)
[![Core Compatibility](https://img.shields.io/badge/esp32--core-2.0.17-orange.svg)](#)

## 1. Preparación del Entorno (Arduino IDE)
Para evitar fallos de sintaxis en funciones de bajo nivel como la purga de memoria de la controladora de radio (`esp_bt_controller_mem_release()`) o en las rutinas de calibración del ADC nativo, es obligatorio homologar el entorno de desarrollo bajo la rama estable **v2.x** de Espressif.

### Paso a Paso para la Instalación del Core:
1. Abra el **Arduino IDE**.
2. Dirígete al menú **Archivo > Preferencias**.
3. En el apartado **Gestor de URLs Adicionales de Tarjetas**, pegue la siguiente dirección JSON:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
4. Navegue a **Herramientas > Placa > Gestor de Tarjetas**.
5. Busque **esp32** e instale estrictamente la versión **2.0.17**.

> ⚠️ **ADVERTENCIA DE COMPATIBILIDAD:** No actualice a las versiones del núcleo 3.x. Las modificaciones en la API interna de red de Espressif alteran el comportamiento de la pila de red `lwIP` y rompen la estabilidad de los WebSockets en este Kernel.

---

## 2. Gestión de Librerías Dependientes
El firmware v3.6.0 implementa una arquitectura orientada a eventos basada en Socket.IO, deserialización JSON asíncrona y procesamiento web no bloqueante. Instale las siguientes dependencias mediante el **Gestor de Librerías** (`Herramientas > Administrar Bibliotecas...`):

### 📁 Cuadro de Dependencias Obligatorias

| # | Librería | Versión Requerida | Propósito y Justificación Técnica |
| :-: | :--- | :---: | :--- |
| **1** | **ArduinoJson** *(Benoit Blanchon)* | **v6.x** *(e.g., v6.21.5)* | **Crítico:** No instale la rama v7.x. La sintaxis del buffer dinámico `DynamicJsonDocument` fue deprecada en la nueva versión y genera un error fatal de compilación. |
| **2** | **WebSockets** *(Markus Sattler)* | **v2.4.1** *(o superior)* | Capa de transporte bidireccional y persistente de ultra-baja latencia sobre la pila TCP nativa. |
| **3** | **SocketIoClient** *(Ivan Conrado)* | **Última Estable** | Implementación del protocolo Socket.IO compatible con los motores v3.x/v4.x del servidor Tally Arbiter. |
| **4** | **WiFiManager** *(tablatronix)* | **v2.0.16-rc** *(o superior)* | Orquestación del Portal Cautivo en segundo plano de manera asíncrona mediante el método no bloqueante `wm.process()`. |

---

## 3. Configuración Crítica del Hardware en el IDE
Antes de ejecutar la directiva de compilación y subida (`Upload`), verifique que el mapa de asignación de memoria Flash y las velocidades físicas de transferencia coincidan exactamente con la siguiente matriz en el menú de **Herramientas**:

* **Placa (Board):** "WEMOS LOLIN32" (o "ESP32 Dev Module")
* **Upload Speed:** "921600" (Reducir a 115200 si el puerto COM pierde sincronía)
* **Flash Frequency:** "80MHz"
* **Flash Mode:** "QIO" (Quad I/O para máxima velocidad de lectura de bus)
* **Partition Scheme:** 🔴 "Minimal SPIFFS (Large APPS with OTA)"
* **Core Debug Level:** "Ninguno" (Para producción limpia) o "Error" (Para entornos de desarrollo)

### 🧠 Análisis del Mapa de Partición (¿Por qué Minimal SPIFFS?)
El binario compilado del firmware tiene un peso neto de **1,047,577 bytes (~1.04 MB)**. 
* Con la partición por defecto, el espacio máximo asignable para la aplicación se ve severamente limitado.
* Al seleccionar **Minimal SPIFFS (Large APPS with OTA)**, el espacio máximo para el ejecutable se expande a **1.96 MB (1,966,080 bytes)**, dejando tu binario en un óptimo **53%** de uso de disco.
* Este colchón de seguridad de **~918 KB libres** es un requerimiento técnico estricto para que la librería **ArduinoOTA** pueda descargar el nuevo firmware en paralelo en la partición secundaria mientras el equipo sigue operando al aire de forma segura.

---

## 4. Diagnóstico y Verificación en el Monitor Serie
Una vez cargado el programa, abra el monitor serie del IDE configurado estrictamente a **115200 baudios** y presione el botón de reinicio físico (`EN/RST`) en la placa. Deberá validar la siguiente secuencia de inicialización del Kernel:

```syslog
[ENERGÍA] Inicializando sistema...
[HARDWARE] Purgando pila Bluetooth Low Energy de forma exitosa.
[MEMORIA] 51 KB inyectados dinámicamente al Heap de ejecución.
[NVS] Leyendo parámetros desde memoria persistente no volátil...
[NVS] Configuración cargada: Host: 192.168.1.150 | Puerto: 4455 | ID: CAM1
[WIFI] Inicializando pila de comunicación asíncrona lwIP.
[SYSTEM] Kernel v1.0 desplegado con éxito.
