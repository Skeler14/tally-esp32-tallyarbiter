#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <Arduino_JSON.h>
#include <Preferences.h>
#include <Ticker.h>

/* VARIABLES DE CONFIGURACIÓN DEL USUARIO */
bool CUT_BUS = true; // true = Program + Preview = Tally Rojo; false = Program + Preview = Tally Amarillo

// Parámetros dinámicos del Tally
String tallyarbiter_host;
int tallyarbiter_port;
String TALLY_DEVICE_ID;
String wifi_ssid = "";
String wifi_pass = "";

/* ASIGNACIÓN DE PINES FÍSICOS */
const int PIN_PROGRAM = 25;  // LED Rojo
const int PIN_PREVIEW = 26;  // LED Verde
const int PIN_STATUS = 33;   // LED Azul de Estado

/* VARIABLES GLOBALES DE DIAGNÓSTICO DE ENERGÍA */
int adcRawValue = 0;
int detectedPin = 35;
bool noMeasurementHardware = false;
bool isCharging = false;

/* CONTROL DE TIEMPOS Y ESTADOS */
unsigned long lastWiFiConnectedTime = 0;
int lastTallyState = -1; 
bool isAPMode = false;

// Estados de control de alertas
bool socketConnected = false;
bool lowBatteryAlert = false;

Preferences preferences;
WebServer server(80);
SocketIOclient socket;
Ticker statusTicker; 

JSONVar BusOptions;
JSONVar Devices;
JSONVar DeviceStates;
String DeviceId = "unassigned";
String DeviceName = "No Asignado";
String ListenerType = "esp32-tally"; 

bool mode_preview = false;
bool mode_program = false;
bool networkConnected = false;

// --- DECLARACIÓN O FUNCIONES EN ORDEN CRÍTICO ---
void ws_emit(String event, String payload = "") {
  String msg = (payload != "") ? "[\"" + event + "\"," + payload + "]" : "[\"" + event + "\"]";
  socket.sendEVENT(msg);
}

void cambiarEstadoLED() {
  digitalWrite(PIN_STATUS, !digitalRead(PIN_STATUS));
}

void iniciarParpadeo(float segundos) {
  statusTicker.detach(); 
  statusTicker.attach(segundos, cambiarEstadoLED);
}

void apagarLED() {
  statusTicker.detach();
  digitalWrite(PIN_STATUS, LOW); 
}

int getBatteryPercentage() {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(35);
    delay(2);
  }
  adcRawValue = sum / 10;
  if (adcRawValue < 500) {
    noMeasurementHardware = true;
    isCharging = false;
    return -1;
  }
  noMeasurementHardware = false;
  if (adcRawValue >= 2320) {
    isCharging = true;
    int percentage = map(adcRawValue, 2320, 2450, 85, 100);
    return constrain(percentage, 0, 100);
  } else {
    isCharging = false;
    int percentage = map(adcRawValue, 1900, 2310, 0, 100);
    return constrain(percentage, 0, 100);
  }
}

void evaluateMode() {
  int currentState = 0; 
  if (mode_preview && !mode_program) currentState = 1;
  else if (!mode_preview && mode_program) currentState = 2;
  else if (mode_preview && mode_program) currentState = 3;

  if (currentState == lastTallyState) return; 
  lastTallyState = currentState;

  if (currentState == 1) { digitalWrite(PIN_PROGRAM, LOW); digitalWrite(PIN_PREVIEW, HIGH); }
  else if (currentState == 2) { digitalWrite(PIN_PROGRAM, HIGH); digitalWrite(PIN_PREVIEW, LOW); }
  else if (currentState == 3) {
    if (CUT_BUS == true) { digitalWrite(PIN_PROGRAM, HIGH); digitalWrite(PIN_PREVIEW, LOW); }
    else { digitalWrite(PIN_PROGRAM, HIGH); digitalWrite(PIN_PREVIEW, HIGH); }
  }
  else { digitalWrite(PIN_PROGRAM, LOW); digitalWrite(PIN_PREVIEW, LOW); }
}

void SetDeviceName() {
  for (int i = 0; i < Devices.length(); i++) {
    if (JSON.stringify(Devices[i]["id"]) == "\"" + DeviceId + "\"") {
      String strDevice = JSON.stringify(Devices[i]["name"]);
      DeviceName = strDevice.substring(1, strDevice.length() - 1);
      break;
    }
  }
  preferences.begin("tally-arbiter", false);
  preferences.putString("devicename", DeviceName);
  preferences.end();
  evaluateMode();
}

String getBusTypeById(String busId) {
  for (int i = 0; i < BusOptions.length(); i++) {
    if (JSON.stringify(BusOptions[i]["id"]) == busId) return JSON.stringify(BusOptions[i]["type"]);
  }
  return "invalid";
}

void processTallyData() {
  String targetDeviceQuote = "\"" + DeviceId + "\""; 
  for (int i = 0; i < DeviceStates.length(); i++) {
    if (JSON.stringify(DeviceStates[i]["deviceId"]) != targetDeviceQuote) continue;
    String busIdStr = JSON.stringify(DeviceStates[i]["busId"]);
    String busType = getBusTypeById(busIdStr);
    if (busType == "\"preview\"") mode_preview = (DeviceStates[i]["sources"].length() > 0);
    if (busType == "\"program\"") mode_program = (DeviceStates[i]["sources"].length() > 0);
  }
  evaluateMode();
}

void socket_Flash() {
  for(int i = 0; i < 4; i++) {
    digitalWrite(PIN_PROGRAM, HIGH); digitalWrite(PIN_PREVIEW, LOW); delay(100);
    digitalWrite(PIN_PROGRAM, LOW); digitalWrite(PIN_PREVIEW, HIGH); delay(100);
  }
  digitalWrite(PIN_PREVIEW, LOW); lastTallyState = -1; evaluateMode();
}

void socket_event(socketIOmessageType_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case sIOtype_CONNECT:
      socketConnected = true;
      if (lowBatteryAlert) iniciarParpadeo(2.0); else apagarLED(); 
      ws_emit("bus_options");
      ws_emit("listenerclient_connect", "{\"deviceId\":\"" + DeviceId + "\",\"listenerType\":\"" + ListenerType + "\",\"canBeReassigned\":true,\"canBeFlashed\":true,\"supportsChat\":false}"); 
      break;
    case sIOtype_DISCONNECT:
    case sIOtype_ERROR:
      socketConnected = false;
      break;
    case sIOtype_EVENT: {
      String msg = (char*)payload;
      String typeStr = msg.substring(2, msg.indexOf("\"", 2));
      String content = msg.substring(typeStr.length() + 4);
      content.remove(content.length() - 1);

      if (typeStr == "bus_options") BusOptions = JSON.parse(content);
      if (typeStr == "flash") socket_Flash();
      if (typeStr == "deviceId") { DeviceId = content.substring(1, content.length()-1); SetDeviceName(); }
      if (typeStr == "devices") { Devices = JSON.parse(content); SetDeviceName(); }
      if (typeStr == "device_states") { DeviceStates = JSON.parse(content); processTallyData(); }
      break;
    }
    default: break;
  }
}

void connectToServer() {
  socket.onEvent(socket_event);
  socket.begin(tallyarbiter_host.c_str(), tallyarbiter_port, "/socket.io/?EIO=3");
}

void apagarTallyPorSoftware() {
  for(int i = 0; i < 10; i++) {
    digitalWrite(PIN_PROGRAM, HIGH); digitalWrite(PIN_PREVIEW, LOW); delay(60);
    digitalWrite(PIN_PROGRAM, LOW); digitalWrite(PIN_PREVIEW, HIGH); delay(60);
  }
  digitalWrite(PIN_PROGRAM, LOW); digitalWrite(PIN_PREVIEW, LOW); digitalWrite(PIN_STATUS, LOW);
  socket.disconnect();
  delay(150);
  esp_deep_sleep_start();
}

void setupWebServer(); 

void iniciarModoAP() {
  isAPMode = true;
  socketConnected = false;
  
  digitalWrite(PIN_PROGRAM, LOW);
  digitalWrite(PIN_PREVIEW, LOW);
  mode_program = false;
  mode_preview = false;
  lastTallyState = 0;

  iniciarParpadeo(0.1); 
  
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA); 
  
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  
  WiFi.softAP("Config-Tally-AP");
  Serial.println("[TALLY] Modo AP iniciado. Entra a http://192.168.4.1");
  
  setupWebServer();
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    int bat = getBatteryPercentage();
    String batHtml = "";
    
    if (isAPMode) {
      batHtml = "<div style='background: #0284c7; padding: 14px; border-radius: 8px; text-align: center; margin-bottom: 24px; border: 1px solid #0369a1;'> "
                "  <span style='color: #ffffff; font-weight: 600; font-size: 15px;'>📡 Modo Configuración AP Activo</span>"
                "  <div style='font-size: 12px; color: #e0f2fe; margin-top: 6px;'>Introduce los datos abajo para conectar el Tally a tu red.</div>"
                "</div>";
    } else if (noMeasurementHardware) {
      batHtml = "<div style='background: #2a1b1f; padding: 14px; border-radius: 8px; text-align: center; margin-bottom: 24px; border: 1px solid #4a2328;'>"
                "  <span style='color: #f87171; font-weight: 600; font-size: 15px;'>🔋 Alimentación: Activa (Modo Seguro)</span>"
                "</div>";
    } else if (isCharging) {
      batHtml = "<div style='background: #112240; padding: 14px; border-radius: 8px; text-align: center; margin-bottom: 24px; border: 1px solid #1d3557;'>"
                "  <span style='color: #64ffda; font-weight: 600; font-size: 16px;'>⚡ Alimentación: USB / Cargando</span>"
                "</div>";
    } else {
      String color = "#10b981"; if (bat <= 15) color = "#ef4444";
      batHtml = "<div style='background: #1e293b; padding: 14px; border-radius: 8px; text-align: center; margin-bottom: 24px; border: 1px solid #334155;'>"
                "  <span style='color: " + color + "; font-weight: 600; font-size: 16px;'>🔋 Batería: " + String(bat) + "%</span>"
                "</div>";
    }

    String html = R"html(
<!DOCTYPE html>
<html lang='es'>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Configuración Tally</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; background-color: #0f111a; color: #e2e8f0; display: flex; flex-direction: column; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px; box-sizing: border-box; }
    .card { background-color: #1a1d29; padding: 30px; border-radius: 16px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); width: 100%; max-width: 420px; box-sizing: border-box; border: 1px solid #2d3142; position: relative; }
    h2 { margin-top: 0; margin-bottom: 20px; color: #3b82f6; font-size: 24px; text-align: center; }
    .form-group { margin-bottom: 16px; }
    label { display: block; margin-bottom: 6px; font-size: 13px; color: #94a3b8; }
    .input-btn-container { position: relative; display: flex; align-items: center; gap: 8px; }
    input[type='text'], input[type='password'] { width: 100%; padding: 11px 14px; border: 1px solid #334155; background-color: #0f111a; color: #ffffff; border-radius: 8px; box-sizing: border-box; font-size: 15px; }
    .btn-inline { background-color: #334155; border: 1px solid #475569; color: white; padding: 11px 14px; border-radius: 8px; cursor: pointer; font-size: 15px; display: flex; align-items: center; justify-content: center; min-width: 46px; box-sizing: border-box; }
    .btn-inline:hover { background-color: #475569; }
    .toggle-pass { position: absolute; right: 12px; cursor: pointer; color: #94a3b8; user-select: none; font-size: 14px; font-weight: bold; }
    input[type='submit'] { width: 100%; padding: 14px; background-color: #10b981; color: #ffffff; border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; margin-top: 10px; }
    input[type='submit']:hover { background-color: #059669; }
    .btn-zone { display: flex; gap: 10px; margin-top: 15px; }
    .btn-action { flex: 1; padding: 12px; border: none; border-radius: 8px; font-size: 13px; font-weight: 600; cursor: pointer; color: white; text-align: center; text-decoration: none; user-select: none; }
    .btn-sleep { background-color: #dc2626; }
    .btn-sleep:hover { background-color: #b91c1c; }
    .btn-wifi { background-color: #0284c7; }
    .btn-wifi:hover { background-color: #0369a1; }
    
    /* MODAL DE ESCANEO */
    .modal { display: none; position: fixed; z-index: 1000; left: 0; top: 0; width: 100%; height: 100%; background-color: rgba(15,17,26,0.85); align-items: center; justify-content: center; padding: 20px; box-sizing: border-box; }
    .modal-content { background-color: #1a1d29; border: 1px solid #334155; border-radius: 16px; width: 100%; max-width: 380px; padding: 24px; box-sizing: border-box; box-shadow: 0 20px 40px rgba(0,0,0,0.6); }
    .modal-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; }
    .modal-title { font-size: 18px; font-weight: 600; color: #3b82f6; }
    .close-modal { color: #94a3b8; font-size: 22px; font-weight: bold; cursor: pointer; user-select: none; }
    .close-modal:hover { color: #ffffff; }
    .wifi-list { max-height: 240px; overflow-y: auto; margin-top: 10px; border-radius: 8px; border: 1px solid #2d3142; }
    .wifi-item { padding: 12px 16px; border-bottom: 1px solid #2d3142; cursor: pointer; display: flex; justify-content: space-between; align-items: center; background-color: #11131c; font-size: 14px; }
    .wifi-item:last-child { border-bottom: none; }
    .wifi-item:hover { background-color: #24283b; color: #64ffda; }
    .loading-text { text-align: center; padding: 20px; color: #94a3b8; font-size: 14px; }

    /* PANTALLA GENERAL DE ACCIÓN / CARGA (LOADING OVERLAY) */
    .loader-overlay { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background-color: #0f111a; z-index: 9999; flex-direction: column; align-items: center; justify-content: center; color: white; }
    .spinner { border: 4px solid rgba(255, 255, 255, 0.1); width: 50px; height: 50px; border-radius: 50%; border-left-color: #3b82f6; animation: spin 1s linear infinite; margin-bottom: 20px; }
    @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
    .loader-title { font-size: 22px; font-weight: 600; margin-bottom: 8px; color: #ffffff; text-align: center; }
    .loader-subtitle { font-size: 14px; color: #94a3b8; text-align: center; max-width: 300px; }
  </style>
  <script>
    function togglePassword() {
      var x = document.getElementById("wifi_password");
      var btn = document.getElementById("toggle_btn");
      if (x.type === "password") { x.type = "text"; btn.innerHTML = "👁️"; } 
      else { x.type = "password"; btn.innerHTML = "🙈"; }
    }

    function abrirBuscadorWiFi() {
      document.getElementById("wifiModal").style.display = "flex";
      var lista = document.getElementById("lista_redes");
      lista.innerHTML = "<div class='loading-text'>⏳ Escaneando redes cercanas...<br><span style='font-size:11px;color:#64748b;'>Esto tomará unos 3 segundos</span></div>";
      
      fetch("/scan")
        .then(response => response.json())
        .then(data => {
          lista.innerHTML = "";
          if(data.length == 0) {
            lista.innerHTML = "<div class='loading-text' style='color:#ef4444;'>No se encontraron redes de 2.4GHz.</div>";
            return;
          }
          data.forEach(function(red) {
            var div = document.createElement("div");
            div.className = "wifi-item";
            div.innerHTML = "<span>" + red.ssid + "</span><span style='color:#94a3b8;font-size:12px;'>" + red.rssi + "% 📶</span>";
            div.onclick = function() {
              document.getElementById("wifi_ssid_input").value = red.ssid;
              cerrarModal();
            };
            lista.appendChild(div);
          });
        })
        .catch(err => {
          lista.innerHTML = "<div class='loading-text' style='color:#ef4444;'>Error al conectar con la placa.</div>";
        });
    }

    function cerrarModal() {
      document.getElementById("wifiModal").style.display = "none";
    }

    function mostrarCargando(titulo, subtitulo) {
      document.getElementById("loader_title").innerText = titulo;
      document.getElementById("loader_subtitle").innerText = subtitulo;
      document.getElementById("loadingOverlay").style.display = "flex";
    }

    function enviarComandoSegundoPlano(ruta, titulo, subtitulo) {
      mostrarCargando(titulo, subtitulo);
      fetch(ruta)
        .then(res => console.log("Comando enviado."))
        .catch(err => console.log("Reinicio procesado correctamente."));
    }

    // NUEVO PARA MÓVILES: Captura los datos y los envía asíncronamente sin romper el HTML móvil
    function guardarConfiguracion(event) {
      event.preventDefault(); // Detiene por completo la redirección del navegador móvil
      
      mostrarCargando('Guardando Datos', 'El Tally se está reiniciando para conectar al nuevo WiFi...');
      
      var formData = new FormData(document.getElementById("configForm"));
      
      fetch("/save", {
        method: "POST",
        body: formData
      })
      .then(res => console.log("Configuración enviada."))
      .catch(err => console.log("La placa se está reiniciando de forma segura."));
    }
  </script>
</head>
<body>
  <div class='card'>
    <h2>Configuración Tally</h2>
    %BATTERY_SECTION%
    <form id='configForm' onsubmit='guardarConfiguracion(event)'>
      <div class='form-group'>
        <label>Nombre de tu red WiFi (SSID):</label>
        <div class='input-btn-container'>
          <input type='text' id='wifi_ssid_input' name='ssid' value='%WIFI_SSID%' placeholder='Ej: MiRedHome_2.4G'>
          <button type='button' class='btn-inline' onclick='abrirBuscadorWiFi()' title='Buscar redes cercanas'>🔍</button>
        </div>
      </div>
      <div class='form-group'>
        <label>Contraseña WiFi:</label>
        <div class='input-btn-container'>
          <input type='password' id='wifi_password' name='pass' value='%WIFI_PASS%'>
          <span id='toggle_btn' class='toggle-pass' onclick='togglePassword()'>🙈</span>
        </div>
      </div>
      <hr style='border-color: #2d3142; margin: 20px 0;'>
      <div class='form-group'>
        <label>IP Servidor Tally Arbiter:</label>
        <input type='text' name='host' value='%TALLY_HOST%'>
      </div>
      <div class='form-group'>
        <label>Puerto Servidor:</label>
        <input type='text' name='port' value='%TALLY_PORT%'>
      </div>
      <div class='form-group'>
        <label>Device ID:</label>
        <input type='text' name='deviceid' value='%TALLY_DEVICE_ID%'>
      </div>
      <input type='submit' value='Guardar y Conectar Tally'>
    </form>
    %EXTRA_BUTTONS%
  </div>

  <div id="wifiModal" class="modal">
    <div class="modal-content">
      <div class="modal-header">
        <span class="modal-title">Redes Disponibles (2.4GHz)</span>
        <span class="close-modal" onclick="cerrarModal()">&times;</span>
      </div>
      <div id="lista_redes" class="wifi-list"></div>
    </div>
  </div>

  <div id="loadingOverlay" class="loader-overlay">
    <div class="spinner"></div>
    <div id="loader_title" class="loader-title">Procesando...</div>
    <div id="loader_subtitle" class="loader-subtitle">Por favor espera un momento.</div>
  </div>

</body>
</html>
)html";

    html.replace("%BATTERY_SECTION%", batHtml);
    html.replace("%WIFI_SSID%", wifi_ssid);
    html.replace("%WIFI_PASS%", wifi_pass);
    html.replace("%TALLY_HOST%", tallyarbiter_host);
    html.replace("%TALLY_PORT%", String(tallyarbiter_port));
    html.replace("%TALLY_DEVICE_ID%", TALLY_DEVICE_ID);

    String extraButtons = "";
    if (!isAPMode) {
      extraButtons = R"html(
      <div class='btn-zone'>
        <button class='btn-action btn-sleep' onclick="enviarComandoSegundoPlano('/gotsleep', '🔴 Apagando Tally', 'El dispositivo está entrando en modo de reposo profundo seguro.')">🔴 Apagar Tally</button>
        <button class='btn-action btn-wifi' onclick="enviarComandoSegundoPlano('/gotreset', '🔄 Abriendo Modo AP', 'Desconectando de la red actual y levantando portal local en http://192.168.4.1')">🔄 Resetear WiFi</button>
      </div>
      )html";
    }
    html.replace("%EXTRA_BUTTONS%", extraButtons);

    server.send(200, "text/html", html);
  });

  server.on("/scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    if (n > 0) {
      for (int i = 0; i < n; ++i) {
        if(WiFi.SSID(i).length() == 0) continue;
        int rssi = WiFi.RSSI(i);
        int quality = 2 * (rssi + 100);
        if(rssi <= -100) quality = 0;
        else if(rssi >= -50) quality = 100;
        
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(quality) + "}";
        if (i < n - 1) json += ",";
      }
      if(json.endsWith(",")) json.remove(json.length() - 1);
    }
    json += "]";
    server.send(200, "application/json", json);
    WiFi.scanDelete(); 
  });

  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid")) wifi_ssid = server.arg("ssid");
    if (server.hasArg("pass")) wifi_pass = server.arg("pass");
    if (server.hasArg("host")) tallyarbiter_host = server.arg("host");
    if (server.hasArg("port")) tallyarbiter_port = server.arg("port").toInt();
    if (server.hasArg("deviceid")) TALLY_DEVICE_ID = server.arg("deviceid");

    preferences.begin("tally-arbiter", false);
    preferences.putString("ssid", wifi_ssid);
    preferences.putString("pass", wifi_pass);
    preferences.putString("host", tallyarbiter_host);
    preferences.putInt("port", tallyarbiter_port);
    preferences.putString("deviceid", TALLY_DEVICE_ID);
    preferences.end();

    server.send(200, "text/plain", "OK");
    delay(1000);
    ESP.restart(); 
  });

  server.on("/gotsleep", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
    delay(500); 
    apagarTallyPorSoftware(); 
  });

  server.on("/gotreset", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
    delay(500);
    iniciarModoAP();
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_PROGRAM, OUTPUT);
  pinMode(PIN_PREVIEW, OUTPUT);
  pinMode(PIN_STATUS, OUTPUT); 
  
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0); 
  
  pinMode(35, INPUT);
  analogSetAttenuation(ADC_11db); 
  
  digitalWrite(PIN_PROGRAM, LOW);
  digitalWrite(PIN_PREVIEW, LOW);
  
  iniciarParpadeo(0.2); 

  setCpuFrequencyMhz(160); 

  preferences.begin("tally-arbiter", false);
  tallyarbiter_host = preferences.getString("host", "192.168.10.107");
  tallyarbiter_port = preferences.getInt("port", 4455);
  TALLY_DEVICE_ID = preferences.getString("deviceid", "0e59aba8");
  wifi_ssid = preferences.getString("ssid", "");
  wifi_pass = preferences.getString("pass", "");
  if(preferences.getString("devicename").length() > 0){
    DeviceName = preferences.getString("devicename");
  }
  preferences.end();

  DeviceId = TALLY_DEVICE_ID;

  if (wifi_ssid.length() > 0) {
    Serial.println("[TALLY] Intentando conectar a: " + wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.status() != WL_CONNECTED) {
    iniciarModoAP();
  } else {
    Serial.println("[TALLY] Conectado exitosamente. IP: " + WiFi.localIP().toString());
    networkConnected = true;
    lastWiFiConnectedTime = millis();
    if (MDNS.begin("configurar-tally1")) {
      MDNS.addService("http", "tcp", 80);
    }
    setupWebServer();
    connectToServer();
  }
}

void loop() {
  server.handleClient();

  if (!isAPMode) {
    if (WiFi.status() == WL_CONNECTED) {
      socket.loop();
    } else {
      if (millis() - lastWiFiConnectedTime >= 30000) {
        iniciarModoAP();
      }
    }

    static unsigned long lastBatteryCheck = 0;
    if (millis() - lastBatteryCheck >= 15000) {
      lastBatteryCheck = millis();
      int bateriaActual = getBatteryPercentage();
      if (!noMeasurementHardware && !isCharging && bateriaActual <= 12) {
        if (!lowBatteryAlert) { lowBatteryAlert = true; iniciarParpadeo(2.0); }
      } else {
        if (lowBatteryAlert) { lowBatteryAlert = false; if (socketConnected) apagarLED(); else iniciarParpadeo(0.2); }
      }
    }
  }
}
