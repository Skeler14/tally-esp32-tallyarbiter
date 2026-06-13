# 📡 MANUAL DE USUARIO
## Sistema Tally Inalámbrico ESP32 para Tally Arbiter

**Versión del Firmware:** v1.1 (Anti-Lag & RF Optimized)  
**Compatible con:** Tally Arbiter 3.x / 4.x  
**Desarrollado por:** Miguel Alegría (WhatsApp: 315 2194064)

---

## 1. Introducción
Gracias por utilizar el sistema Tally inalámbrico basado en el hardware **LOLIN32 (ESP32)**. Este dispositivo recibe de forma nativa y en tiempo real las señales de conmutación desde el servidor de Tally Arbiter para indicar los estados de las cámaras en el set:
* **PROGRAM:** Dispositivo en vivo (Al aire).
* **PREVIEW:** Dispositivo en previo (Próximo a salir).

### Características Clave de la Versión v1.1:
* **Arquitectura Anti-Lag:** Procesamiento lineal optimizado que elimina por completo los congelamientos ante transiciones rápidas de switchers.
* **Frecuencia Forzada a 240MHz:** El procesador opera a máxima potencia para evitar cuellos de botella.
* **Potencia Inalámbrica Continua:** Inhabilita los modos de reposo del módem de radio (`WIFI_PS_NONE`) manteniendo el enlace estable incluso si la batería baja a 3.6V.
* **Panel Web AJAX:** Configuración ágil y lectura de batería bajo demanda sin recargar la página.

---

## 2. Descripción de los Indicadores LED
El estado del hardware se reporta visualmente a través de tres canales de luz independientes:

### 🔴 LED ROJO (PROGRAM)
Indica que la cámara o dispositivo está actualmente al aire en el Switcher de video.
* **Encendido Fijo:** Dispositivo en PROGRAM (Al Aire).
* **Apagado:** Fuera del aire.

### 🟢 LED VERDE (PREVIEW)
Indica que la cámara o dispositivo está seleccionado en el bus de previo.
* **Encendido Fijo:** Dispositivo en PREVIEW.
* **Apagado:** Fuera de previo.

### 🔵 LED AZUL (STATUS / BATERÍA)
Reporta los estados del sistema de red y alertas críticas de energía:
* **Parpadeo Rápido (cada 0.2s):** Buscando o intentando conectarse a la red WiFi guardada.
* **Parpadeo Muy Rápido (cada 0.1s):** No encontró redes conocidas y activó el Punto de Acceso Local de Configuración.
* **Parpadeo Lento (cada 2.0s):** ⚠️ **Alerta Crítica de Energía.** Nivel de batería al **10% o inferior**. Requiere recarga inmediata.
* **Apagado:** Conectado al servidor, operando con normalidad y nivel de batería óptimo.

---

## 3. Encendido e Inicialización del Sistema
El dispositivo arranca automáticamente en cuanto recibe alimentación por el puerto USB o el interruptor de la batería. 

Al encender, ejecuta la siguiente secuencia interna:
1. Lee las credenciales de red y el identificador desde la memoria persistente no volátil (NVS).
2. Forza el reloj de la CPU a 240MHz y realiza un muestreo inicial del voltaje.
3. Inicia un parpadeo de diagnóstico en el LED azul (0.2s) e intenta engancharse a la red WiFi.
4. Si se conecta, estabiliza el socket de datos, apaga el LED azul y queda en escucha aérea.

---

## 4. Primera Configuración (Portal de Rescate)
Si el dispositivo es nuevo, ha sido reseteado, o la red WiFi del set no está disponible, el Tally desplegará su propio portal de configuración inalámbrica:

### Pasos para Configurar:
1. Desde un teléfono móvil o computadora, conéctese a la red WiFi abierta generada por la placa llamada:  
   `Config-Tally-AP`
2. Abra cualquier navegador web (Chrome, Safari, etc.) e ingrese de forma manual la siguiente dirección IP en la barra de navegación:  
   `192.168.4.1`
3. En la interfaz web que aparece, complete los siguientes campos:
   * **SSID:** Nombre de la red WiFi del estudio (puede usar el botón de la lupa 🔍 para escanear redes de 2.4GHz en vivo).
   * **Contraseña WiFi:** Clave de acceso a la red.
   * **IP Servidor Tally Arbiter:** Dirección de la computadora o servidor central.
   * **Puerto Servidor:** Puerto de escucha de Tally Arbiter (por defecto es `4455`).
   * **Device ID:** El identificador asignado a esta unidad de cámara (Ej: `CAM1`, `PTZ_STAGE`, `STEADICAM`).
4. Presione el botón **"Guardar y Conectar Tally"**. El equipo almacenará los datos de forma permanente y se reiniciará de inmediato para acoplarse a la producción.

---

## 5. El Panel de Control Web v1.1
Cuando el Tally ya se encuentra operando dentro de la red del estudio, puede ingresar a su panel web de mantenimiento escribiendo en el navegador de su computadora la dirección:  
`http://configurar-tally1.local/` (o la dirección IP que su router le haya asignado).

Desde este panel asíncrono podrá realizar las siguientes acciones en caliente:
* **Módulo de Batería Inteligente:** Para proteger los hilos de comunicación de red de las interferencias de la antena, el microcontrolador no lee el pin analógico de forma constante. La web mostrará el último estado estático. Para conocer el voltaje real al instante, presione el botón **"Ver nivel de batería (Dar para actualizar)"**, el cual ejecutará una petición AJAX rápida sin interrumpir la transmisión.
* **Apagar Tally:** Duerme el chip por software enviándolo a modo *Deep Sleep* profundo para transporte seguro.
* **Resetear WiFi:** Fuerza al dispositivo a abandonar la red actual y levantar el punto de acceso local de rescate.

---

## 6. Modos de Operación Física (Botón BOOT)
El botón físico marcado como `BOOT` (GPIO 0) integrado en la placa permite interactuar directamente con el hardware:

1. **Apagado Manual (Deep Sleep):** Mantenga presionado el botón `BOOT` por **5 segundos**. Los LEDs se apagarán por completo y el chip entrará en hibernación extrema. Para despertarlo, presione una vez el botón `BOOT` o retire y vuelva a colocar la alimentación.
2. **Restablecimiento de Fábrica:** Mantenga presionado el botón `BOOT` por **10 segundos**. El Tally borrará de su memoria interna todas las redes guardadas, las IPs registradas y el Device ID. Al reiniciarse, parpadeará rápidamente el LED azul y levantará la red abierta `Config-Tally-AP`.

---

## 7. Actualizaciones inalámbricas (OTA)
El firmware del dispositivo cuenta con soporte de actualización por aire (Over-The-Air) mediante la librería *ArduinoOTA*. Cuando se inyecta un nuevo bloque de código por red local:
* Los LEDs de **PROGRAM** (Rojo) y **PREVIEW** (Verde) permanecerán encendidos simultáneamente de forma fija.
* El dispositivo no debe perder alimentación ni apagarse bajo ninguna circunstancia mientras dure la barra de carga.
* Al finalizar la carga, el equipo aplicará los cambios y se reiniciará automáticamente.

---

## 8. Solución de Problemas Rápidos

### El LED Azul parpadea de forma constante (0.2s) y no recibe señal
* **Causa:** El equipo no puede conectarse al WiFi del estudio o se cayó el router.
* **Solución:** Compruebe que la red de 2.4GHz del estudio esté encendida. Si cambió la contraseña, deje presionado el botón `BOOT` por 10 segundos para volver a configurarlo.

### El LED Azul está apagado pero los LEDs de Tally no reaccionan al Switcher
* **Causa:** El Tally está conectado correctamente al WiFi, pero no logra enlazar con el servidor de video, la IP/Puerto están mal escritos, o el `Device ID` no coincide con el asignado en la interfaz de Tally Arbiter.
* **Solución:** Entre al panel web del dispositivo o al portal de rescate y verifique que el campo `Device ID` sea idéntico en mayúsculas y minúsculas al nombre del dispositivo configurado en el servidor de Tally Arbiter.

### Los LEDs PROGRAM y PREVIEW parpadean de un lado a otro alternadamente
* **Causa:** El dispositivo ha recibido una instrucción de destello (*Flash*) remota desde el servidor de Tally Arbiter para su identificación física en el set de grabación.
* **Solución:** Operación normal. El Tally volverá automáticamente a su estado actual en 1 segundo.

---

## 9. Especificaciones Técnicas
* **Plataforma Core:** ESP32-D0WDQ6 (Dual Core a 240MHz fijo).
* **Red Inalámbrica:** Wi-Fi 802.11 b/g/n (2.4 GHz) con antena integrada.
* **Protocolo de Datos:** Socket.IO v3 / WebSockets persistentes de baja latencia.
* **Gestión de Energía:** Desactivación de ahorro de energía RF (`WIFI_PS_NONE`) y corte por software a sueño profundo.
* **Asignación de Pines:** PROGRAM: GPIO 25 | PREVIEW: GPIO 26 | STATUS/BAT: GPIO 33 | ADC Batt: GPIO 35.
