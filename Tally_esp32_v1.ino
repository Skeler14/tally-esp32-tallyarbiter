#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <Arduino_JSON.h>
#include <Preferences.h>
#include <Ticker.h>
#include <esp_wifi.h>

/* VARIABLES DE CONFIGURACIÓN DEL USUARIO */
bool CUT_BUS = true; 

String tallyarbiter_host;
int tallyarbiter_port;
String TALLY_DEVICE_ID;
String wifi_ssid = "";
String wifi_pass = "";
String device_id_list = "0e59aba8"; 

// CONFIGURACIÓN DE PINES
const int PIN_BUTTON = 0;    // Pin del pulsador (Pulsado = LOW)
const int PIN_PROGRAM = 25;  
const int PIN_PREVIEW = 26;  
const int PIN_STATUS = 33;   

// Variables optimizadas para el control del botón (Tiempos reducidos a la mitad)
unsigned long buttonPressStartTime = 0;
bool isButtonPressed = false;
bool action4SecTriggered = false; 

int adcRawValue = 0;
bool noMeasurementHardware = false;
bool isCharging = false;
int ultimoPorcentajeBateria = -1; 

unsigned long lastWiFiConnectedTime = 0;
int lastTallyState = -1; 
bool isAPMode = false;

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
  for (int i = 0; i < 5; i++) {
    sum += analogRead(35);
  }
  adcRawValue = sum / 5;
  if (adcRawValue < 500) {
    noMeasurementHardware = true;
    isCharging = false;
    ultimoPorcentajeBateria = -1;
    return -1;
  }
  noMeasurementHardware = false;
  if (adcRawValue >= 2320) {
    isCharging = true;
    int percentage = map(adcRawValue, 2320, 2450, 85, 100);
    ultimoPorcentajeBateria = constrain(percentage, 0, 100);
    return ultimoPorcentajeBateria;
  } else {
    isCharging = false;
    int percentage = map(adcRawValue, 1900, 2310, 0, 100);
    ultimoPorcentajeBateria = constrain(percentage, 0, 100);
    return ultimoPorcentajeBateria;
  }
}

String generarHtmlBateria() {
  if (ultimoPorcentajeBateria == -1 && !noMeasurementHardware && !isCharging) {
    return "<div id='batBox' style='background: #1e293b; padding: 14px; border-radius: 8px; text-align: center; margin-bottom: 24px; border: 1px solid #334155;'>"
           "  <span style='color: #94a3b8; font-weight: 600; font-size: 16px;'>🔋 Batería: Sin consultar</span>"
           "</div>";
  }
  if (noMeasurementHardware) {
    return "<div id='batBox' style='background: #2a1b1f; padding: 14px; border-radius: 8px; text-align: center; margin-bottom: 24px; border: 1px solid #4a2328;'>"
           "  <span style='color: #f87171; font-weight: 600; font-size: 15px;'>🔋 Alimentación: Activa (Modo Seguro)</span>"
           "</div>";
  }
  if (isCharging) {
    return "<div id='batBox' style='background: #112240; padding: 14px; border-radius: 8px; text-align: center; margin-bottom: 24px; border: 1px solid #1d3557;'>"
           "  <span style='color: #64ffda; font-weight: 600; font-size: 16px;'>⚡ Alimentación: USB / Cargando (" + String(ultimoPorcentajeBateria) + "%)</span>"
           "</div>";
  }
  
  String color = "#10b981"; 
  if (ultimoPorcentajeBateria <= 10) color = "#ef4444";
  else if (ultimoPorcentajeBateria <= 25) color = "#f59e0b";

  return "<div id='batBox' style='background: #1e293b; padding: 14px; border-radius: 8px; text-align: center; margin-bottom: 24px; border: 1px solid #334155;'>"
         "  <span style='color: " + color + "; font-weight: 600; font-size: 16px;'>🔋 Batería: " + String(ultimoPorcentajeBateria) + "%</span>"
         "</div>";
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
    if (CUT_BUS) { digitalWrite(PIN_PROGRAM, HIGH); digitalWrite(PIN_PREVIEW, LOW); }
    else { digitalWrite(PIN_PROGRAM, HIGH); digitalWrite(PIN_PREVIEW, HIGH); }
  }
  else { digitalWrite(PIN_PROGRAM, LOW); digitalWrite(PIN_PREVIEW, LOW); }
}

void SetDeviceName() {
  int len = Devices.length();
  for (int i = 0; i < len; i++) {
    if (JSON.stringify(Devices[i]["id"]) == "\"" + DeviceId + "\"") {
      String strDevice = JSON.stringify(Devices[i]["name"]);
      DeviceName = strDevice.substring(1, strDevice.length() - 1);
      break;
    }
  }
  
  preferences.begin("tally-names", false);
  preferences.putString(DeviceId.c_str(), DeviceName);
  preferences.end();

  preferences.begin("tally-arbiter", false);
  preferences.putString("devicename", DeviceName);
  preferences.end();
  evaluateMode();
}

void processTallyData() {
  String targetDeviceQuote = "\"" + DeviceId + "\""; 
  int statesLen = DeviceStates.length();
  int busLen = BusOptions.length();
  
  bool new_preview = false;
  bool new_program = false;

  for (int i = 0; i < statesLen; i++) {
    if (JSON.stringify(DeviceStates[i]["deviceId"]) != targetDeviceQuote) continue;
    
    String busIdStr = JSON.stringify(DeviceStates[i]["busId"]);
    bool sourcesActive = (DeviceStates[i]["sources"].length() > 0);
    
    for (int j = 0; j < busLen; j++) {
      if (JSON.stringify(BusOptions[j]["id"]) == busIdStr) {
        String busType = JSON.stringify(BusOptions[j]["type"]);
        if (busType == "\"preview\"") new_preview = sourcesActive;
        else if (busType == "\"program\"") new_program = sourcesActive;
        break; 
      }
    }
  }

  mode_preview = new_preview;
  mode_program = new_program;
  evaluateMode();
}

void socket_Flash() {
  for(int i = 0; i < 4; i++) {
    digitalWrite(PIN_PROGRAM, HIGH); digitalWrite(PIN_PREVIEW, LOW); delay(250);
    digitalWrite(PIN_PROGRAM, LOW); digitalWrite(PIN_PREVIEW, HIGH); delay(250);
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
      int firstQuote = msg.indexOf("\"");
      if (firstQuote == -1) return;
      int secondQuote = msg.indexOf("\"", firstQuote + 1);
      if (secondQuote == -1) return;
      
      String typeStr = msg.substring(firstQuote + 1, secondQuote);
      String content = msg.substring(secondQuote + 2);
      if(content.endsWith("]")) content.remove(content.length() - 1);

      if (typeStr == "device_states") { DeviceStates = JSON.parse(content); processTallyData(); }
      else if (typeStr == "devices") { Devices = JSON.parse(content); SetDeviceName(); }
      if (typeStr == "bus_options") { BusOptions = JSON.parse(content); }
      else if (typeStr == "flash") { socket_Flash(); }
      else if (typeStr == "deviceId") { 
        if(content.startsWith("\"") && content.endsWith("\"")) DeviceId = content.substring(1, content.length()-1);
        else DeviceId = content;
        SetDeviceName(); 
      }
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
  Serial.println("[SYSTEM] Esperando a que se suelte el boton para apagar seguro...");
  while(digitalRead(PIN_BUTTON) == LOW) {
    delay(10);
  }
  
  Serial.println("[SYSTEM] Boton liberado. Apagando ahora.");
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

  iniciarParpadeo(0.5); 
  
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP); 
  
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  
  WiFi.softAP("TallyConfig");
  delay(200);

  MDNS.end();
  if (MDNS.begin("configurar-tally1")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] Modo AP Listo en http://configurar-tally1.local");
  }
  
  setupWebServer();
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String batHtml = "";
    if (isAPMode) {
      batHtml = "<div style='background: #0284c7; padding: 14px; border-radius: 8px; text-align: center; margin-bottom: 24px; border: 1px solid #0369a1;'> "
                "  <span style='color: #ffffff; font-weight: 600; font-size: 15px;'>📡 Modo Configuración AP Activo</span>"
                "</div>";
    } else {
      batHtml = generarHtmlBateria();
    }

    String selectOptionsHtml = "";
    String listaClon = device_id_list;
    
    preferences.begin("tally-names", true); 

    while (listaClon.length() > 0) {
      int index = listaClon.indexOf(',');
      String idItem = "";
      if (index == -1) {
        idItem = listaClon;
        listaClon = "";
      } else {
        idItem = listaClon.substring(0, index);
        listaClon = listaClon.substring(index + 1);
      }
      idItem.trim();
      if (idItem.length() > 0) {
        String selectedAttr = (idItem == TALLY_DEVICE_ID) ? "selected" : "";
        
        String savedName = preferences.getString(idItem.c_str(), "");
        String displayName = "";
        
        if (idItem == TALLY_DEVICE_ID && DeviceName != "No Asignado") {
          displayName = DeviceName + " (" + idItem + ")";
        } else if (savedName.length() > 0) {
          displayName = savedName + " (" + idItem + ")";
        } else {
          displayName = idItem; 
        }
        
        selectOptionsHtml += "<option value='" + idItem + "' " + selectedAttr + ">" + displayName + "</option>";
      }
    }
    preferences.end();

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
    input[type='text'], input[type='password'], select { width: 100%; padding: 11px 14px; border: 1px solid #334155; background-color: #0f111a; color: #ffffff; border-radius: 8px; box-sizing: border-box; font-size: 15px; }
    select { appearance: none; cursor: pointer; background-image: url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" fill="white" viewBox="0 0 24 24"><path d="M7 10l5 5 5-5z"/></svg>'); background-repeat: no-repeat; background-position: right 12px center; padding-right: 40px; }
    .btn-inline { background-color: #334155; border: 1px solid #475569; color: white; padding: 11px 14px; border-radius: 8px; cursor: pointer; font-size: 15px; display: flex; align-items: center; justify-content: center; min-width: 46px; box-sizing: border-box; user-select: none; }
    .btn-inline:hover { background-color: #475569; }
    .btn-delete { background-color: #7f1d1d; border-color: #991b1b; }
    .btn-delete:hover { background-color: #991b1b; }
    .toggle-pass { position: absolute; right: 12px; cursor: pointer; color: #94a3b8; user-select: none; font-size: 14px; font-weight: bold; }
    input[type='submit'] { width: 100%; padding: 14px; background-color: #10b981; color: #ffffff; border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; margin-top: 10px; }
    input[type='submit']:hover { background-color: #059669; }
    
    .btn-zone { display: flex; flex-direction: column; gap: 10px; margin-top: 15px; }
    .btn-row { display: flex; gap: 10px; }
    .btn-action { flex: 1; padding: 12px; border: none; border-radius: 8px; font-size: 13px; font-weight: 600; cursor: pointer; color: white; text-align: center; text-decoration: none; user-select: none; }
    .btn-sleep { background-color: #dc2626; }
    .btn-sleep:hover { background-color: #b91c1c; }
    .btn-wifi { background-color: #0284c7; }
    .btn-wifi:hover { background-color: #0369a1; }
    .btn-check-bat { background-color: #475569; border: 1px solid #64748b; width: 100%; padding: 11px; margin-bottom: 15px; font-size: 13px; }
    .btn-check-bat:hover { background-color: #64748b; }
    
    /* Estilos del Bloque de Créditos */
    .credits-box { margin-top: 25px; text-align: center; font-size: 13.5px; color: #94a3b8; border-top: 1px solid #2d3142; padding-top: 15px; line-height: 1.6; }
    .credits-link { color: #25d366; text-decoration: none; font-weight: 600; }
    .credits-link:hover { text-decoration: underline; }

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
      lista.innerHTML = "<div class='loading-text'>⏳ Escaneando redes cercanas...</div>";
      
      fetch("/scan")
        .then(response => response.json())
        .then(data => {
          lista.innerHTML = "";
          if(data.length == 0) {
            lista.innerHTML = "<div class='loading-text' style='color:#ef4444;'>No se encontraron redes.</div>";
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
        });
    }

    function cerrarModal() { document.getElementById("wifiModal").style.display = "none"; }

    function mostrarCargando(titulo, subtitulo) {
      document.getElementById("loader_title").innerText = titulo;
      document.getElementById("loader_subtitle").innerText = subtitulo;
      document.getElementById("loadingOverlay").style.display = "flex";
    }

    // Ejecuta comandos y retorna al menú principal de forma segura
    function enviarComandoSegundoPlano(ruta, titulo, subtitulo) {
      mostrarCargando(titulo, subtitulo);
      fetch(ruta)
        .then(() => procesarRetornoAcciones(ruta))
        .catch(() => procesarRetornoAcciones(ruta));
    }

    function procesarRetornoAcciones(ruta) {
      if (ruta === '/gotsleep') {
        setTimeout(function() { window.close(); }, 4000);
      } else if (ruta === '/gotreset') {
        setTimeout(function() { window.location.href = 'http://192.168.4.1'; }, 3000);
      } else {
        setTimeout(function() { window.location.reload(); }, 1500);
      }
    }

    function borrarIdSeleccionado() {
      var select = document.getElementById("device_select");
      var idABorrar = select.value;
      if (confirm("¿Estás seguro de que quieres borrar el ID '" + idABorrar + "' del desplegable?")) {
        mostrarCargando("Borrando ID", "Actualizando lista...");
        fetch("/deleteid?id=" + encodeURIComponent(idABorrar))
          .then(() => { setTimeout(function() { window.location.reload(); }, 1000); });
      }
    }

    function copiarSelectAInput() {
      var select = document.getElementById("device_select");
      document.getElementById("device_input").value = select.value;
    }

    function guardarConfiguracion(event) {
      event.preventDefault(); 
      mostrarCargando('Guardando Datos', 'El Tally se está reiniciando...');
      var formData = new FormData(document.getElementById("configForm"));
      fetch("/save", { method: "POST", body: formData })
        .then(() => {
          setTimeout(function() { window.location.href = window.location.origin; }, 3500);
        })
        .catch(() => {
          setTimeout(function() { window.location.href = window.location.origin; }, 3500);
        });
    }

    function consultarBateriaManual() {
      var btn = document.getElementById("btnBat");
      var originalText = btn.innerText;
      btn.innerText = "⏳ Midiendo voltaje...";
      btn.disabled = true;

      fetch("/getbat")
        .then(response => response.text())
        .then(htmlBloque => {
          document.getElementById("batContainer").innerHTML = htmlBloque;
          btn.innerText = originalText;
          btn.disabled = false;
        })
        .catch(err => {
          btn.innerText = "❌ Error al medir";
          btn.disabled = false;
        });
    }
  </script>
</head>
<body>
  <div class='card'>
    <h2>Configuración Tally</h2>
    
    <div id="batContainer">
      %BATTERY_SECTION%
    </div>

    %BUTTON_CHECK_BAT%

    <form id='configForm' onsubmit='guardarConfiguracion(event)'>
      <div class='form-group'>
        <label>Nombre de tu red WiFi (SSID):</label>
        <div class='input-btn-container'>
          <input type='text' id='wifi_ssid_input' name='ssid' value='%WIFI_SSID%'>
          <button type='button' class='btn-inline' onclick='abrirBuscadorWiFi()'>🔍</button>
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
        <label>Seleccionar ID existente en memoria:</label>
        <div class='input-btn-container'>
          <select id='device_select' onchange='copiarSelectAInput()'>
            %SELECT_OPTIONS%
          </select>
          <button type='button' class='btn-inline btn-delete' title='Borrar este ID' onclick='borrarIdSeleccionado()'>🗑️</button>
        </div>
      </div>

      <div class='form-group'>
        <label>Device ID activo (Escribe uno nuevo o usa el desplegable):</label>
        <input type='text' id='device_input' name='deviceid' value='%TALLY_DEVICE_ID%' placeholder='Ej: CAM1' required>
        <span style='font-size: 13px; font-weight: 500; color: #f1f5f9; margin-top: 6px; display: block;'>Vinculado a red: <b style='color: #3b82f6;'>%DEVICE_NAME%</b></span>
      </div>
      
      <input type='submit' value='Guardar y Conectar Tally'>
    </form>
    %EXTRA_BUTTONS%
    
    <div class='credits-box'>
      Desarrollado por: <b>Miguel Alegria</b><br>
      Soporte técnico: <a class='credits-link' href='https://wa.me/573015329313' target='_blank' rel='noopener noreferrer'>+57 3015329313 (Solo WhatsApp)</a>
    </div>
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
    html.replace("%DEVICE_NAME%", DeviceName); 
    html.replace("%SELECT_OPTIONS%", selectOptionsHtml);

    if (!isAPMode) {
      html.replace("%BUTTON_CHECK_BAT%", "<button id='btnBat' class='btn-action btn-check-bat' onclick='consultarBateriaManual()'>Ver nivel de batería (Dar para actualizar)</button>");
      
      String extraButtons = R"html(
      <div class='btn-zone'>
        <div class='btn-row'>
          <button class='btn-action btn-sleep' onclick="enviarComandoSegundoPlano('/gotsleep', '🔴 Apagando Tally', 'Entrando en modo de reposo...')">🔴 Apagar Tally</button>
          <button class='btn-action btn-wifi' onclick="enviarComandoSegundoPlano('/gotreset', '🔄 Abriendo Modo AP', 'Levantando portal local...')">🔄 Resetear WiFi</button>
        </div>
      </div>
      )html";
      html.replace("%EXTRA_BUTTONS%", extraButtons);
    } else {
      html.replace("%BUTTON_CHECK_BAT%", "");
      html.replace("%EXTRA_BUTTONS%", "");
    }

    server.send(200, "text/html", html);
  });

  server.on("/getbat", HTTP_GET, []() {
    getBatteryPercentage(); 
    server.send(200, "text/html", generarHtmlBateria());
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

  server.on("/deleteid", HTTP_GET, []() {
    if (server.hasArg("id")) {
      String idABorrar = server.arg("id");
      idABorrar.trim();
      
      String nuevaLista = "";
      String listaClon = device_id_list;
      while (listaClon.length() > 0) {
        int index = listaClon.indexOf(',');
        String idItem = "";
        if (index == -1) {
          idItem = listaClon;
          listaClon = "";
        } else {
          idItem = listaClon.substring(0, index);
          listaClon = listaClon.substring(index + 1);
        }
        idItem.trim();
        if (idItem.length() > 0 && idItem != idABorrar) {
          if (nuevaLista.length() > 0) nuevaLista += ",";
          nuevaLista += idItem;
        }
      }
      
      if(nuevaLista.length() == 0) nuevaLista = "0e59aba8"; 
      device_id_list = nuevaLista;
      
      preferences.begin("tally-arbiter", false);
      preferences.putString("idlist", device_id_list);
      if (TALLY_DEVICE_ID == idABorrar) {
        int primerComa = device_id_list.indexOf(',');
        TALLY_DEVICE_ID = (primerComa == -1) ? device_id_list : device_id_list.substring(0, primerComa);
        preferences.putString("deviceid", TALLY_DEVICE_ID);
        preferences.putString("devicename", "No Asignado");
      }
      preferences.end();

      preferences.begin("tally-names", false);
      preferences.remove(idABorrar.c_str());
      preferences.end();
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid")) wifi_ssid = server.arg("ssid");
    if (server.hasArg("pass")) wifi_pass = server.arg("pass");
    if (server.hasArg("host")) tallyarbiter_host = server.arg("host");
    if (server.hasArg("port")) tallyarbiter_port = server.arg("port").toInt();
    
    if (server.hasArg("deviceid")) {
      TALLY_DEVICE_ID = server.arg("deviceid");
      TALLY_DEVICE_ID.trim();
      
      bool existe = false;
      String listaClon = device_id_list;
      while (listaClon.length() > 0) {
        int index = listaClon.indexOf(',');
        String idItem = (index == -1) ? listaClon : listaClon.substring(0, index);
        listaClon = (index == -1) ? "" : listaClon.substring(index + 1);
        idItem.trim();
        if (idItem == TALLY_DEVICE_ID) { existe = true; break; }
      }
      
      if (!existe && TALLY_DEVICE_ID.length() > 0) {
        if (device_id_list.length() > 0) device_id_list += ",";
        device_id_list += TALLY_DEVICE_ID;
      }
    }

    preferences.begin("tally-arbiter", false);
    preferences.putString("ssid", wifi_ssid);
    preferences.putString("pass", wifi_pass);
    preferences.putString("host", tallyarbiter_host);
    preferences.putInt("port", tallyarbiter_port);
    preferences.putString("deviceid", TALLY_DEVICE_ID);
    preferences.putString("idlist", device_id_list);
    preferences.putString("devicename", "Actualizando..."); 
    preferences.end();

    server.send(200, "text/plain", "OK");
    delay(500);
    ESP.restart(); 
  });

  server.on("/gotsleep", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
    delay(200); 
    apagarTallyPorSoftware(); 
  });

  server.on("/gotreset", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
    delay(200);
    iniciarModoAP();
  });

  server.stop(); 
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(600); 
  
  Serial.println("\n[WIFI] Inicializando en modo Estacion (STA)...");
  
  pinMode(PIN_PROGRAM, OUTPUT);
  pinMode(PIN_PREVIEW, OUTPUT);
  pinMode(PIN_STATUS, OUTPUT); 
  
  pinMode(PIN_BUTTON, INPUT_PULLUP); 
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0); 
  
  pinMode(35, INPUT);
  analogSetAttenuation(ADC_11db); 
  
  digitalWrite(PIN_PROGRAM, LOW);
  digitalWrite(PIN_PREVIEW, LOW);
  
  iniciarParpadeo(0.2); 
  setCpuFrequencyMhz(240); 

  preferences.begin("tally-arbiter", false);
  tallyarbiter_host = preferences.getString("host", "192.168.10.107");
  tallyarbiter_port = preferences.getInt("port", 4455);
  TALLY_DEVICE_ID = preferences.getString("deviceid", "0e59aba8"); 
  wifi_ssid = preferences.getString("ssid", "");
  wifi_pass = preferences.getString("pass", "");
  device_id_list = preferences.getString("idlist", "0e59aba8");
  if(preferences.getString("devicename").length() > 0){
    DeviceName = preferences.getString("devicename");
  }
  preferences.end();

  DeviceId = TALLY_DEVICE_ID;
  getBatteryPercentage();

  if (wifi_ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(250);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    iniciarModoAP();
  } else {
    networkConnected = true;
    lastWiFiConnectedTime = millis();
    
    WiFi.setSleep(false); 
    esp_wifi_set_ps(WIFI_PS_NONE); 
    
    MDNS.end();
    if (MDNS.begin("configurar-tally1")) {
      MDNS.addService("http", "tcp", 80);
    }
    
    setupWebServer();
    connectToServer();
  }
}

void loop() {
  server.handleClient();

  // --- CONTROL DEL BOTÓN FÍSICO CON TIEMPOS OPTIMIZADOS A LA MITAD ---
  int buttonState = digitalRead(PIN_BUTTON);
  
  if (buttonState == LOW) { // Botón presionado
    if (!isButtonPressed) {
      isButtonPressed = true;
      buttonPressStartTime = millis();
      action4SecTriggered = false;
    }
    
    unsigned long pressDuration = millis() - buttonPressStartTime;
    
    // Si cruza los 4 segundos, el RESET de WiFi se dispara de inmediato (Modo AP)
    if (pressDuration >= 4000 && !action4SecTriggered) {
      action4SecTriggered = true;
      Serial.println("[BUTTON] Detectados 4 segundos. Abriendo Modo AP...");
      iniciarModoAP();
    }
  } 
  else { // El botón se suelta (HIGH)
    if (isButtonPressed) {
      unsigned long finalDuration = millis() - buttonPressStartTime;
      isButtonPressed = false; 
      
      // Si se soltó entre los 1.5 y los 4 segundos, apaga el Tally de forma segura
      if (finalDuration >= 1500 && finalDuration < 4000 && !action4SecTriggered) {
        Serial.println("[BUTTON] Soltado tras 1.5+ segundos. Apagando Tally...");
        apagarTallyPorSoftware();
      }
    }
  }
  // ------------------------------------------------------------------

  if (!isAPMode) {
    if (WiFi.status() == WL_CONNECTED) {
      socket.loop();
    } else {
      if (millis() - lastWiFiConnectedTime >= 20000) {
        iniciarModoAP();
      }
    }

    static unsigned long lastBatteryCheck = 0;
    if (millis() - lastBatteryCheck >= 300000) { 
      lastBatteryCheck = millis();
      int bateriaActual = getBatteryPercentage();
      
      if (!noMeasurementHardware && !isCharging && bateriaActual <= 10 && bateriaActual > 0) {
        if (!lowBatteryAlert) { 
          lowBatteryAlert = true; 
          iniciarParpadeo(2.0); 
        }
      } else {
        if (lowBatteryAlert) { 
          lowBatteryAlert = false; 
          if (socketConnected) apagarLED(); else iniciarParpadeo(0.2); 
        }
      }
    }
  }
}
