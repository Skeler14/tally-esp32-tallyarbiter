# 📡 MANUAL DE USUARIO: ESP32 Wireless Tally System

Operational guide for the ESP32 Tally system integrated with Tally Arbiter.

[![Firmware Version](https://img.shields.io/badge/Firmware-v1-blue.svg)](#)
[![Compatibility](https://img.shields.io/badge/Compatible%20with-Tally%20Arbiter%203.x--4.x-orange.svg)](#)

---

## 1. Introducción 📖
Gracias por utilizar el sistema Tally inalámbrico basado en el hardware **LOLIN32 (ESP32)**. Este dispositivo recibe de forma nativa y en tiempo real las señales de conmutación desde el servidor de Tally Arbiter para indicar los estados de las cámaras en el set:

* **PROGRAM:** Dispositivo en vivo (Al aire) 🔴
* **PREVIEW:** Dispositivo en previo (Próximo a salir) 🟢

### Características Clave de la Versión v1 ⚙️:
* **Arquitectura Anti-Lag:** Procesamiento lineal optimizado que elimina por completo los congelamientos ante transiciones rápidas de switchers.
* **Frecuencia Forzada a 240MHz:** El procesador opera a máxima potencia para evitar cuellos de botella ⚡.
* **Potencia Inalámbrica Continua:** Inhabilita los modos de reposo del módem de radio (`WIFI_PS_NONE`) manteniendo el enlace estable incluso si la batería baja a 3.6V 📶.
* **Panel Web AJAX:** Configuración ágil y lectura de batería bajo demanda sin recargar la página 🌐.

---

## 2. Descripción de los Indicadores LED 💡
El estado del hardware se reporta visualmente a través de tres canales de luz independientes:

| LED | Estado Físico | Significado Operativo |
| :--- | :--- | :--- |
| **🔴 ROJO (PROGRAM)** | Encendido Fijo | La cámara está actualmente **AL AIRE**. |
| **🟢 VERDE (PREVIEW)** | Encendido Fijo | La cámara está seleccionada en **PREVIO**. |
| **🔵 AZUL (STATUS)** | Parpadeo Rápido (0.2s) | Buscando o intentando conectarse a la red WiFi 🔍. |
| | Parpadeo Muy Rápido (0.1s) | No encontró redes; activo Punto de Acceso Local. |
| | Parpadeo Lento (2.0s) | ⚠️ **Alerta Crítica:** Batería al 10% o menos 🔋. |
| | Apagado | Conectado, operación normal y batería óptima ✅. |

---

## 3. Encendido e Inicialización ⚡
El dispositivo arranca automáticamente al recibir alimentación. Secuencia interna:
1. Lee las credenciales de red y el identificador desde la memoria persistente (NVS).
2. Forza el reloj de la CPU a 240MHz y realiza un muestreo inicial del voltaje 📊.
3. Inicia un parpadeo de diagnóstico en el LED azul e intenta engancharse a la red WiFi.
4. Si se conecta, estabiliza el socket de datos y queda en escucha aérea.

---

## 4. Primera Configuración (Portal de Rescate) 🛠️
Si el dispositivo no encuentra red, desplegará su propio portal:

1. Conéctese a la red WiFi abierta: `Config-Tally-AP` 📶.
2. Ingrese a la IP: `192.168.4.1` en su navegador 🌐.
3. Complete los campos:
   * **SSID:** Nombre de la red WiFi (use la lupa 🔍 para escanear).
   * **Contraseña WiFi:** Clave de acceso 🔑.
   * **IP Servidor Tally Arbiter:** Dirección del servidor central.
   * **Puerto Servidor:** (por defecto `4455`).
   * **Device ID:** Identificador de la cámara (Ej: `CAM1`) 🏷️.
4. Presione **"Guardar y Conectar Tally"**. El equipo se reiniciará 🔄.

---

## 5. Panel de Control Web v1.1 🖥️
Acceda mediante: `http://configurar-tally1.local/`

* **Batería:** Presione **"Ver nivel de batería"** para una petición AJAX rápida 🔋.
* **Apagar Tally:** Envía el chip a modo *Deep Sleep* para transporte seguro 💤.
* **Resetear WiFi:** Regresa al modo de configuración inicial 🔄.

---

## 6. Modos de Operación Física (Botón BOOT) 🔘
1. **Apagado Manual (Deep Sleep):** Mantenga presionado el botón `BOOT` por **5 segundos**. Para despertar, presione una vez el botón o reconecte la alimentación.
2. **Restablecimiento de Fábrica:** Mantenga presionado por **10 segundos**. El Tally borrará toda la memoria y volverá a `Config-Tally-AP` 🧼.

---

## 7. Actualizaciones inalámbricas (OTA) 📡
* Los LEDs de **PROGRAM** (Rojo) y **PREVIEW** (Verde) permanecerán encendidos fijos durante la carga.
* ¡No desconecte la alimentación mientras dure la carga! ⚠️
* Al finalizar, el equipo se reiniciará automáticamente.

---

## 8. Solución de Problemas 🛠️
* **LED Azul parpadea (0.2s) y no recibe señal:** Equipo no conectado al WiFi. Compruebe la red o presione BOOT 10s para reconfigurar 🔍.
* **LED Azul apagado pero no reacciona al Switcher:** Verifique que el `Device ID` coincida exactamente en Tally Arbiter.
* **LEDs parpadean alternadamente:** El dispositivo recibió una instrucción de destello (*Flash*) para su identificación física. Operación normal.

---

## 9. Especificaciones Técnicas 📋
* **Plataforma:** ESP32-D0WDQ6 (Dual Core 240MHz fijo) 🧠.
* **Red:** Wi-Fi 802.11 b/g/n (2.4 GHz) 📶.
* **Protocolo:** Socket.IO v3 / WebSockets persistentes 🔗.
* **Pines:** PROGRAM: GPIO 25 | PREVIEW: GPIO 26 | STATUS/BAT: GPIO 33 | ADC Batt: GPIO 35.

---
**Desarrollado por:** Miguel Alegría (WhatsApp: 315 2194064) 💬
