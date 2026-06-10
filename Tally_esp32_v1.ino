/*
 * ============================================================
 * TALLY ARBITER — ESP32 Firmware v1.0
 * Hardware: LOLIN32 (ESP32)
 * Compatible: Tally Arbiter 3.x (Socket.IO sobre WebSocket)
 * Autor: Miguel Alegria
 * ============================================================
 */

#include <WiFi.h>
#include <SocketIOclient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <esp_adc_cal.h>
#include "driver/adc.h"   // FIX: adc1_config_width / adc1_get_raw viven aquí. Sin esto no compila en algunos cores 3.x.
#include <esp_sleep.h>
#include "esp_bt.h"
#include "esp_wifi.h" // Necesario para el control directo de la radio PHY

// ============================================================
// PINES
// ============================================================
const int PIN_PROGRAM  = 25;
const int PIN_PREVIEW  = 26;
const int PIN_BAT_LED  = 27;
const int PIN_BAT_ADC  = 35;
const int PIN_BOOT_BTN = 0;

// ============================================================
// CONFIGURACIÓN DE TIMERS Y THRESHOLDS
// ============================================================
const unsigned long DEEP_SLEEP_TIMEOUT_MS       = 10UL * 60UL * 1000UL;
const unsigned long INITIAL_CONN_TIMEOUT_MS     = 45UL * 1000UL;
const uint64_t      DEEP_SLEEP_WAKE_INTERVAL_US = 30ULL * 1000000ULL;
const unsigned long LINK_WATCHDOG_TIMEOUT_MS    = 20000UL;

const int  BATTERY_LOW_THRESHOLD  = 10;
const float VDIV_FACTOR            = 2.0f;
const unsigned long BATTERY_INTERVAL_MS = 60000;

const int TA_DEFAULT_PORT = 4455;

const unsigned long BLINK_INTERVAL     = 500;
const unsigned long RED_BLINK_INTERVAL = 200;
const unsigned long BASE_RECONNECT_INTERVAL = 5000; // Intervalo base para Backoff

const unsigned long BTN_DEBOUNCE_MS     = 50;
const unsigned long BTN_SHUTDOWN_MS     = 5000;
const unsigned long BTN_FORCE_PORTAL_MS = 10000;

// ============================================================
// ESTRUCTURAS Y MEMORIA GLOBAL
// ============================================================
SocketIOclient socketIO;
Preferences   prefs;
WiFiManager    wm;
esp_adc_cal_characteristics_t adcChars;

// DEFINICIÓN ANTICIPADA DEL ENUM (Resuelve el error de compilación en el IDE de Arduino)
enum AnimState : uint8_t { ANIM_NONE = 0, ANIM_CONN_SUCCESS, ANIM_FLASH };

char ta_host[64]   = "192.168.10.104";
char device_id[32] = "IRIUN1";
int  ta_port       = TA_DEFAULT_PORT;
char portStr[8];

WiFiManagerParameter param_host("host",   "IP TallyArbiter", ta_host,   64);
WiFiManagerParameter param_devid("devId", "Device ID",       device_id, 32);
WiFiManagerParameter param_port("port",  "Puerto",          portStr,   6);

struct BusEntry { char id[48]; char type[16]; };
BusEntry busMap[16]; 
int      busMapCount = 0;

// Buffer estático de uso exclusivo en el hilo síncrono (Core 1)
DynamicJsonDocument doc(4096);

// Mutex nativo para sincronizar accesos a variables compartidas entre Cores
portMUX_TYPE idMux = portMUX_INITIALIZER_UNLOCKED;

const char* getBusTypeById(const char* busId) {
    for (int i = 0; i < busMapCount; i++) {
        if (strcmp(busMap[i].id, busId) == 0) return busMap[i].type;
    }
    return "";
}

// ============================================================
// MÁQUINA DE ESTADOS Y ANIMACIONES
// ============================================================
AnimState    currentAnim  = ANIM_NONE;
int          animStep     = 0;
unsigned long animLastMs  = 0;
const int     ANIM_CONN_STEPS  = 6;
const int     ANIM_FLASH_STEPS = 6;
const unsigned long ANIM_STEP_MS = 150;

void startAnim(AnimState anim) {
    currentAnim = anim;
    animStep    = 0;
    animLastMs  = millis();
}

bool isConnected           = false;
bool everConnectedThisBoot = false;
bool isTallyProgram        = false;
bool isTallyPreview        = false;
bool batteryLow            = false;
bool lastProgramState      = false;
bool lastPreviewState      = false;

// Timers de control independientes
unsigned long lastWiFiAliveTime    = 0; 
unsigned long lastSocketAliveTime  = 0; 
unsigned long lastBatteryCheck     = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long currentReconnectInterval = BASE_RECONNECT_INTERVAL; // Intervalo dinámico
int           wifiRetryCount       = 0;

// Banderas de reinicio diferido seguro
bool          pendingRestart       = false;
unsigned long restartTimer         = 0;

// Control de flujo para actualizaciones OTA activas
bool          isOtaUpdating        = false;

unsigned long btnPressStart = 0;
bool          btnHeld       = false;

unsigned long lastBlinkTime    = 0; bool blinkState    = false;
unsigned long lastRedBlinkTime = 0; bool redBlinkState = false;

// ============================================================
// ADC Y FILTRADO ANALÓGICO NATIVO
// ============================================================
void initADC() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11); 
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adcChars);
}

int getBatteryPercentage() {
    const int total_samples = 12;
    uint32_t samples[total_samples];
    
    for (int i = 0; i < total_samples; i++) { 
        samples[i] = adc1_get_raw(ADC1_CHANNEL_7); 
        delayMicroseconds(50); 
    }
    
    // Filtro de ordenamiento de burbuja para remover picos de ruido RF
    for (int i = 1; i < total_samples; i++) {
        uint32_t key = samples[i];
        int j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }
    
    uint32_t raw_filtered = 0;
    for (int i = 2; i < total_samples - 2; i++) {
        raw_filtered += samples[i];
    }
    raw_filtered /= (total_samples - 4);
    
    // FIX: 'mv' era uint32_t. Con batería <3.5V, (mv-3500) hacía underflow sin signo y el % salía basura.
    // Pasado a int con signo para que el tramo bajo dé 0% real en vez de un número enorme.
    int mv = (int)(esp_adc_cal_raw_to_voltage(raw_filtered, &adcChars) * VDIV_FACTOR);
    return constrain((mv - 3500) * 100 / (4150 - 3500), 0, 100);
}

// ============================================================
// CONTROLADOR DE LEDS
// ============================================================
void updateLEDs() {
    unsigned long now = millis();

    if (currentAnim != ANIM_NONE) {
        if (now - animLastMs >= ANIM_STEP_MS) {
            animLastMs = now;
            bool on = (animStep % 2 == 0);
            int totalSteps = (currentAnim == ANIM_CONN_SUCCESS) ? ANIM_CONN_STEPS : ANIM_FLASH_STEPS;

            if (currentAnim == ANIM_CONN_SUCCESS) {
                digitalWrite(PIN_PREVIEW, on ? HIGH : LOW);
                digitalWrite(PIN_PROGRAM, LOW);
            } else { 
                digitalWrite(PIN_PROGRAM, on ? HIGH : LOW);
                digitalWrite(PIN_PREVIEW, on ? LOW  : HIGH);
            }

            if (++animStep >= totalSteps) {
                currentAnim = ANIM_NONE;
                animStep    = 0;
                digitalWrite(PIN_PROGRAM, isTallyProgram ? HIGH : LOW);
                digitalWrite(PIN_PREVIEW, isTallyPreview  ? HIGH : LOW);
            }
        }
    } else {
        if (!isConnected) {
            if (now - lastRedBlinkTime >= RED_BLINK_INTERVAL) {
                lastRedBlinkTime = now; redBlinkState = !redBlinkState;
                digitalWrite(PIN_PROGRAM, redBlinkState ? HIGH : LOW);
            }
            digitalWrite(PIN_PREVIEW, LOW);
        } else {
            digitalWrite(PIN_PROGRAM, isTallyProgram ? HIGH : LOW);
            digitalWrite(PIN_PREVIEW, isTallyPreview  ? HIGH : LOW);
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (now - lastBlinkTime >= BLINK_INTERVAL) {
            lastBlinkTime = now; blinkState = !blinkState;
            digitalWrite(PIN_BAT_LED, blinkState ? HIGH : LOW);
        }
    } else {
        digitalWrite(PIN_BAT_LED, batteryLow ? HIGH : LOW);
    }
}

// ============================================================
// GESTIÓN DE CONFIGURACIÓN NO VOLÁTIL (NVS)
// ============================================================
void loadConfig() {
    prefs.begin("tally", true);
    portENTER_CRITICAL(&idMux);
    prefs.getString("host",  ta_host,   sizeof(ta_host));
    prefs.getString("devId", device_id, sizeof(device_id));
    ta_port = prefs.getInt("port", ta_port);
    portEXIT_CRITICAL(&idMux);
    prefs.end();
}

void saveConfig(const char* host, const char* devId, int port) {
    prefs.begin("tally", false);
    prefs.putString("host", host);
    prefs.putString("devId", devId);
    prefs.putInt("port", port);
    prefs.end();
}

void resetConfig() { prefs.begin("tally", false); prefs.clear(); prefs.end(); }

// Callback aislado ejecutado desde el hilo HTTP asíncrono
void saveParamsCallback() {
    int p = atoi(param_port.getValue());
    
    portENTER_CRITICAL(&idMux);
    saveConfig(param_host.getValue(), param_devid.getValue(), p);
    strlcpy(ta_host, param_host.getValue(), sizeof(ta_host));
    strlcpy(device_id, param_devid.getValue(), sizeof(device_id));
    ta_port = p;
    portEXIT_CRITICAL(&idMux);

    Serial.println("[CONFIG] Configuración inyectada en NVS de forma segura.");
    
    pendingRestart = true;
    restartTimer = millis();
}

void shutdownConfigPortal() {
    wm.stopConfigPortal(); WiFi.softAPdisconnect(true); delay(100); WiFi.mode(WIFI_STA);
}

// ============================================================
// GESTIÓN DE ENERGÍA CRÍTICA
// ============================================================
void enterDeepSleep() {
    Serial.println("[ENERGÍA] Modo Deep Sleep por inactividad activa.");
    digitalWrite(PIN_PROGRAM, LOW); digitalWrite(PIN_PREVIEW, LOW); digitalWrite(PIN_BAT_LED, LOW);
    shutdownConfigPortal(); socketIO.disconnect();
    WiFi.disconnect(false, false); WiFi.mode(WIFI_OFF); delay(200);
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_WAKE_INTERVAL_US);
    esp_deep_sleep_start();
}

void enterShutdown() {
    Serial.println("[ENERGÍA] Apagado manual solicitado.");
    digitalWrite(PIN_PROGRAM, LOW); digitalWrite(PIN_PREVIEW, LOW); digitalWrite(PIN_BAT_LED, LOW);
    shutdownConfigPortal(); socketIO.disconnect();
    WiFi.disconnect(false, false); WiFi.mode(WIFI_OFF); delay(300);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT_BTN, 0);
    esp_deep_sleep_start();
}

// ============================================================
// EMISIÓN DE PROTOCOLO TALLY ARBITER
// ============================================================
void ws_emit(const char* event) {
    String msg = String("[\"") + event + "\"]";
    socketIO.sendEVENT(msg);
}

void sendListenerConnect() {
    // FIX CRÍTICO: el servidor lee obj.deviceId / obj.listenerType / obj.canBe... (UN OBJETO).
    // El código anterior mandaba argumentos sueltos -> TA recibía deviceId = undefined,
    // el equipo nunca quedaba registrado y nunca llegaban los device_states (no encendía).
    // Wire correcto:  ["listenerclient_connect", { ... }]
    StaticJsonDocument<256> txDoc;
    JsonArray arr = txDoc.to<JsonArray>();
    arr.add("listenerclient_connect");
    JsonObject o = arr.createNestedObject();

    portENTER_CRITICAL(&idMux);
    o["deviceId"] = device_id;
    portEXIT_CRITICAL(&idMux);

    o["listenerType"]    = "esp32-tally";
    o["canBeReassigned"] = true;
    o["canBeFlashed"]    = true;
    o["supportsChat"]    = false;

    String msg;
    serializeJson(txDoc, msg);
    socketIO.sendEVENT(msg);
}

void sendBatteryStatus() {
    if (!isConnected) return;
    int level = getBatteryPercentage();
    batteryLow = (level <= BATTERY_LOW_THRESHOLD);

    StaticJsonDocument<256> txDoc;
    JsonArray arr = txDoc.to<JsonArray>();
    arr.add("device_battery");
    JsonObject p = arr.createNestedObject();
    
    portENTER_CRITICAL(&idMux);
    p["deviceId"] = device_id;
    portEXIT_CRITICAL(&idMux);
    
    p["batteryLevel"] = level;

    String msg;
    serializeJson(txDoc, msg);
    socketIO.sendEVENT(msg);
    Serial.printf("[BATERÍA] Capacidad: %d%%\n", level);
}

// ============================================================
// RECEPCIÓN DE PROTOCOLO TALLY ARBITER
// ============================================================
void processBusOptions(JsonArray arr) {
    busMapCount = 0;
    for (JsonObject bus : arr) {
        if (busMapCount >= 16) break;
        
        const char* b_id = bus["id"].as<const char*>();
        const char* b_ty = bus["type"].as<const char*>();
        
        strlcpy(busMap[busMapCount].id,   b_id ? b_id : "", sizeof(busMap[busMapCount].id));
        strlcpy(busMap[busMapCount].type, b_ty ? b_ty : "", sizeof(busMap[busMapCount].type));
        busMapCount++;
    }
    Serial.printf("[TA] Configuración de buses: %d mapeados.\n", busMapCount);
}

void processDeviceStates(JsonArray arr) {
    isTallyProgram = false;
    isTallyPreview  = false;
    for (JsonObject state : arr) {
        JsonArray sources = state["sources"].as<JsonArray>();
        if (sources.isNull() || sources.size() == 0) continue;

        const char* t = getBusTypeById(state["busId"] | "");
        if (strcmp(t, "program") == 0) isTallyProgram = true;
        if (strcmp(t, "preview") == 0) isTallyPreview  = true;
    }
    if (isTallyProgram != lastProgramState || isTallyPreview != lastPreviewState) {
        Serial.printf("[TALLY] Program: %d | Preview: %d\n", isTallyProgram, isTallyPreview);
        lastProgramState = isTallyProgram;
        lastPreviewState = isTallyPreview;
    }
}

// ============================================================
// MANEJADOR DE EVENTOS DE SOCKET SÍNCRONOS
// ============================================================
void socketIOEvent(socketIOmessageType_t type, uint8_t* payload, size_t length) {
    lastSocketAliveTime = millis(); 

    switch (type) {
        case sIOtype_DISCONNECT:
            isConnected = false; isTallyProgram = false; isTallyPreview = false;
            currentAnim = ANIM_NONE;
            Serial.println("[SIO] Desconectado del servidor principal.");
            break;

        case sIOtype_CONNECT:
            shutdownConfigPortal();
            isConnected = true; everConnectedThisBoot = true;
            startAnim(ANIM_CONN_SUCCESS);
            // FIX: eliminado el socketIO.send(sIOtype_CONNECT,"/") manual. La librería ya
            // negocia el namespace ANTES de disparar este evento; reenviarlo era redundante.
            // Orden correcto: primero registrar el listener, después pedir el mapa de buses.
            sendListenerConnect();
            ws_emit("bus_options");
            sendBatteryStatus();
            Serial.println("[SIO] Enlace Socket.IO operacional.");
            break;

        case sIOtype_EVENT: {
            if (isOtaUpdating) break; 
            
            // Guard Clause defensivo contra desbordamiento masivo
            if (length > 4096) {
                Serial.printf("[SIO] Descarte defensivo: Payload excesivo de %d bytes.\n", length);
                break;
            }
            
            doc.clear(); 
            DeserializationError err = deserializeJson(doc, payload, length);
            if (err) break;

            const char* event = doc[0] | "";

            if (strcmp(event, "bus_options") == 0) {
                processBusOptions(doc[1].as<JsonArray>());
            }
            else if (strcmp(event, "device_states") == 0) {
                processDeviceStates(doc[1].as<JsonArray>());
            }
            else if (strcmp(event, "flash") == 0) {
                startAnim(ANIM_FLASH);
            }
            else if (strcmp(event, "reassign") == 0) {
                String oldIdStr = doc[1]["oldDeviceId"] | "";
                String newIdStr = doc[1]["newDeviceId"] | "";
                
                portENTER_CRITICAL(&idMux);
                bool idMatch = (oldIdStr == device_id);
                portEXIT_CRITICAL(&idMux);

                if (idMatch && newIdStr.length() > 0) {
                    portENTER_CRITICAL(&idMux);
                    strlcpy(device_id, newIdStr.c_str(), sizeof(device_id)); 
                    portEXIT_CRITICAL(&idMux);
                    
                    saveConfig(ta_host, device_id, ta_port);

                    doc.clear();
                    JsonArray ack = doc.to<JsonArray>();
                    ack.add("listener_reassign_object");
                    JsonObject o = ack.createNestedObject();
                    o["oldDeviceId"] = oldIdStr;
                    o["newDeviceId"] = newIdStr;
                    String ackMsg;
                    serializeJson(doc, ackMsg);
                    socketIO.sendEVENT(ackMsg);

                    sendListenerConnect();
                }
            }
            else if (strcmp(event, "deviceId") == 0) {
                const char* newId = doc[1] | "";
                if (strlen(newId) > 0 && strlen(newId) < sizeof(device_id)) {
                    portENTER_CRITICAL(&idMux);
                    strlcpy(device_id, newId, sizeof(device_id)); 
                    portEXIT_CRITICAL(&idMux);
                    
                    saveConfig(ta_host, device_id, ta_port);
                    sendListenerConnect();
                }
            }
            break;
        }
        default:
            break;
    }
}

// ============================================================
// ENTRADAS DIGITALES (BOTÓN BOOT)
// ============================================================
void handleButton() {
    bool pressed = (digitalRead(PIN_BOOT_BTN) == LOW);
    if (pressed && !btnHeld)  { btnHeld = true; btnPressStart = millis(); }
    if (!pressed && btnHeld) {
        unsigned long held = millis() - btnPressStart;
        btnHeld = false;
        if (held < BTN_DEBOUNCE_MS) return;
        if (held >= BTN_FORCE_PORTAL_MS) {
            resetConfig(); wm.resetSettings(); delay(500); ESP.restart();
        } else if (held >= BTN_SHUTDOWN_MS) {
            enterShutdown();
        }
    }
}

// ============================================================
// INITIALIZATION
// ============================================================
void setup() {
    Serial.begin(115200);
    
    // Purga de memoria Bluetooth para anexar 51KB dinámicos reales al Heap
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);

    pinMode(PIN_PROGRAM,  OUTPUT);
    pinMode(PIN_PREVIEW,  OUTPUT);
    pinMode(PIN_BAT_LED,  OUTPUT);
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
    digitalWrite(PIN_PROGRAM, LOW);
    digitalWrite(PIN_PREVIEW, LOW);
    digitalWrite(PIN_BAT_LED, LOW);

    initADC();
    loadConfig();
    snprintf(portStr, sizeof(portStr), "%d", ta_port);

    // FIX: los WiFiManagerParameter se construyen (globales) ANTES de loadConfig(), así que
    // mostraban los valores por defecto del .ino y no los guardados en NVS. Los refrescamos
    // aquí para que el portal muestre el host/devId/puerto que realmente están en uso.
    param_host.setValue(ta_host, sizeof(ta_host));
    param_devid.setValue(device_id, sizeof(device_id));
    param_port.setValue(portStr, 6);

    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(180);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setTitle("Tally Arbiter ESP32");
    wm.addParameter(&param_host);
    wm.addParameter(&param_devid);
    wm.addParameter(&param_port);
    wm.autoConnect("TallyConfig");

    // Limita la potencia Tx a 15dBm para suprimir armónicos RF sobre el ADC de la batería
    WiFi.setTxPower(WIFI_POWER_15dBm);
    
    lastWiFiAliveTime   = millis();
    lastSocketAliveTime = millis();

    // Configuración limpia de la máquina de estados de ArduinoOTA
    ArduinoOTA.setHostname("tally-esp32");
    
    ArduinoOTA.onStart([]() {
        isOtaUpdating = true;
        socketIO.disconnect(); // Cierre del socket de aplicación; lwIP permanece abierto para binario
        digitalWrite(PIN_PROGRAM, HIGH);
        digitalWrite(PIN_PREVIEW, HIGH);
        Serial.println("[OTA] Socket de aplicación cerrado de forma limpia. Descargando firmware...");
    });

    ArduinoOTA.onError([](ota_error_t error) {
        isOtaUpdating = false;
        digitalWrite(PIN_PROGRAM, LOW);
        digitalWrite(PIN_PREVIEW, LOW);
        Serial.printf("[OTA] Error de transmisión [%u]. Reanudando loop secundario de control...\n", error);
    });
    
    ArduinoOTA.begin();

    socketIO.begin(ta_host, ta_port);
    socketIO.onEvent(socketIOEvent);
    socketIO.setReconnectInterval(5000);

    Serial.printf("[SYSTEM] Kernel v3.6.0 desplegado con éxito.\n");
}

// ============================================================
// BUCLE PRINCIPAL (ARQUITECTURA DE TRES CAPAS DEFENSIVAS)
// ============================================================
void loop() {
    // Capa 0: Bloqueo de infraestructura por actualización OTA activa
    if (isOtaUpdating) {
        ArduinoOTA.handle();
        return; 
    }

    unsigned long now = millis();

    // Desplazamiento fuera de hilo del reinicio diferido solicitado por el Portal
    if (pendingRestart && (now - restartTimer >= 1500)) {
        ESP.restart();
    }

    handleButton();
    wm.process();

    if (wm.getConfigPortalActive()) {
        lastWiFiAliveTime = now;
        lastSocketAliveTime = now;
    }

    // --- CAPA 1: AUDITORÍA DE ENLACE E INFRAESTRUCTURA WI-FI (CON BACKOFF EXPONENCIAL) ---
    if (WiFi.status() != WL_CONNECTED) {
        if (isConnected) { isConnected = false; isTallyProgram = false; isTallyPreview = false; }
        
        if (now - lastReconnectAttempt >= currentReconnectInterval) {
            lastReconnectAttempt = now;
            wifiRetryCount++;
            
            Serial.printf("[WIFI] Desconectado. Intento #%d. Reintento en %lu ms...\n", wifiRetryCount, currentReconnectInterval);
            
            // Duplicar intervalo para el siguiente intento (Límite: 60 segundos)
            currentReconnectInterval = min(currentReconnectInterval * 2, 60000UL);
            
            // Hard Reset de la capa de radio física si lwIP está severamente degradado
            if (wifiRetryCount >= 6) {
                Serial.println("[WIFI] Stack inestable persistente. Reiniciando PHY + lwIP...");
                
                socketIO.disconnect();
                WiFi.disconnect(false, false); 
                delay(100);
                
                esp_wifi_stop();
                delay(300);
                esp_wifi_start();
                delay(200);
                
                WiFi.mode(WIFI_STA);
                WiFi.begin(); 
                wifiRetryCount = 0;
                currentReconnectInterval = BASE_RECONNECT_INTERVAL; // Reset del backoff
            } else {
                WiFi.reconnect();
            }
        }
        updateLEDs();

        if (!everConnectedThisBoot && (now - lastWiFiAliveTime >= INITIAL_CONN_TIMEOUT_MS)) enterDeepSleep();
        if (everConnectedThisBoot && (now - lastWiFiAliveTime >= DEEP_SLEEP_TIMEOUT_MS)) enterDeepSleep();
        return;
    }

    // Infraestructura saludable: Reseteo de contadores reactivos y del intervalo base
    lastWiFiAliveTime = now;
    wifiRetryCount = 0;
    currentReconnectInterval = BASE_RECONNECT_INTERVAL;

    // --- CAPA 2: PROCESAMIENTO Y AGENTES SECUNDARIOS SÍNCRONOS ---
    socketIO.loop();
    ArduinoOTA.handle();
    updateLEDs();

    if (now - lastBatteryCheck >= BATTERY_INTERVAL_MS) { 
        lastBatteryCheck = now; 
        sendBatteryStatus(); 
    }

    // --- CAPA 3: WATCHDOG DE APLICACIÓN (MITIGACIÓN CONTRA TCP HALF-OPEN / CAPA 7) ---
    if (isConnected && (now - lastSocketAliveTime >= LINK_WATCHDOG_TIMEOUT_MS)) {
        Serial.println("[WATCHDOG] Servidor silencioso. Forzando desconexión lógica rápida...");
        isConnected = false; isTallyProgram = false; isTallyPreview = false;
        lastSocketAliveTime = now; // Margen de gracia para reenganche sin causar deep sleep prematuro
        socketIO.disconnect();
    }

    // Ahorro de energía si el AP responde pero la instancia de Tally Arbiter desaparece de la red
    unsigned long timeout = everConnectedThisBoot ? DEEP_SLEEP_TIMEOUT_MS : INITIAL_CONN_TIMEOUT_MS;
    if (!isConnected && (now - lastSocketAliveTime >= timeout)) enterDeepSleep();
}