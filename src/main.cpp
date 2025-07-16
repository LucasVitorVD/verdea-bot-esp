#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

ESP8266WebServer server(80);
WiFiManager wifiManager;

// Configurações da API
const char *apiUrl = "http://192.168.15.115:8080/api/device/add";
String lastSentMac = "";
bool alreadyRegistered = false;

void checkOfflineMode();

// Controle de conectividade
unsigned long lastConnectionAttempt = 0;
unsigned long lastHeartbeat = 0;
unsigned long connectionTimeout = 30000; // 30 segundos para timeout
bool isOnlineMode = false;
int reconnectAttempts = 0;
const int maxReconnectAttempts = 5;

// Sistema de irrigação offline (fallback)
bool offlineMode = false;
unsigned long lastIrrigationCheck = 0;
const unsigned long irrigationInterval = 60000; // Verificar a cada 1 minuto
bool pumpStatus = false;
int soilMoisture = 0;

// Configurações padrão para modo offline
const int DEFAULT_MOISTURE_THRESHOLD = 30;
const int DEFAULT_IRRIGATION_DURATION = 30000; // 30 segundos
unsigned long irrigationStartTime = 0;
bool irrigationActive = false;

void sendDeviceRegistration()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("❌ WiFi não conectado - não é possível registrar dispositivo");
    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    WiFiClient client;
    http.setTimeout(10000); // 10 segundos timeout

    http.begin(client, String(apiUrl));
    http.addHeader("Content-Type", "application/json");

    String mac = WiFi.macAddress();
    Serial.print("📡 Tentando registrar dispositivo - MAC: ");
    Serial.println(mac);

    String macSuffix = mac;
    macSuffix.replace(":", "");
    macSuffix = macSuffix.substring(6);
    String deviceName = "irrigacao-verdea-" + macSuffix;

    String payload = "{\"name\": \"" + deviceName + "\", \"macAddress\": \"" + mac + "\", \"status\": \"ONLINE\", \"type\": \"irrigation\"}";

    int httpCode = http.POST(payload);

    if (httpCode >= 200 && httpCode < 300)
    {
      Serial.println("✅ Dispositivo registrado na API!");
      Serial.print("Código HTTP: ");
      Serial.println(httpCode);

      String response = http.getString();
      Serial.println("Resposta da API: " + response);

      alreadyRegistered = true;
      lastSentMac = mac;
      isOnlineMode = true;
      reconnectAttempts = 0;
      offlineMode = false;

      Serial.println("🌐 MODO ONLINE ativado");
    }
    else if (httpCode == 409 || httpCode == 422)
    {
      Serial.println("⚠️ Dispositivo já registrado");
      alreadyRegistered = true;
      isOnlineMode = true;
      reconnectAttempts = 0;
      offlineMode = false;
    }
    else
    {
      Serial.print("❌ Erro na requisição HTTP: ");
      Serial.println(httpCode);
      Serial.println("Resposta: " + http.getString());

      // Se falhar, considerar modo offline
      checkOfflineMode();
    }

    http.end();
  }
}

void checkOfflineMode()
{
  reconnectAttempts++;
  Serial.println("🔄 Tentativa de reconexão: " + String(reconnectAttempts) + "/" + String(maxReconnectAttempts));

  if (reconnectAttempts >= maxReconnectAttempts)
  {
    Serial.println("🚨 Máximo de tentativas atingido - ATIVANDO MODO OFFLINE");
    offlineMode = true;
    isOnlineMode = false;
    reconnectAttempts = 0;

    Serial.println("🌱 Sistema funcionará em modo autônomo");
    Serial.println("   - Irrigação baseada em sensor de umidade");
    Serial.println("   - Limite: " + String(DEFAULT_MOISTURE_THRESHOLD) + "%");
    Serial.println("   - Duração: " + String(DEFAULT_IRRIGATION_DURATION / 1000) + " segundos");
  }
}

void sendHeartbeat()
{
  if (WiFi.status() == WL_CONNECTED && isOnlineMode)
  {
    HTTPClient http;
    WiFiClient client;
    http.setTimeout(5000); // 5 segundos timeout para heartbeat

    String heartbeatUrl = "http://192.168.15.115:8080/api/device/heartbeat";
    http.begin(client, heartbeatUrl);
    http.addHeader("Content-Type", "application/json");

    String mac = WiFi.macAddress();
    String payload = "{\"macAddress\": \"" + mac + "\", \"status\": \"ONLINE\", \"timestamp\": " + String(millis()) + "}";

    int httpCode = http.POST(payload);

    if (httpCode > 0 && httpCode < 400)
    {
      Serial.println("💓 Heartbeat enviado com sucesso");
      lastHeartbeat = millis();
    }
    else
    {
      Serial.println("❌ Falha no heartbeat - HTTP: " + String(httpCode));
      // Se falhar heartbeat, verificar se deve entrar em modo offline
      if (millis() - lastHeartbeat > 300000) // 5 minutos sem heartbeat
      {
        Serial.println("🚨 Sem comunicação com servidor - ativando modo offline");
        checkOfflineMode();
      }
    }

    http.end();
  }
}

// Simulação de leitura do sensor de umidade
int readSoilMoisture()
{
  // Aqui você colocaria a leitura real do sensor
  // return analogRead(SOIL_SENSOR_PIN);

  // Simulação para teste
  return random(0, 100);
}

void controlPump(bool turnOn)
{
  pumpStatus = turnOn;

  // Aqui você controlaria o relé real
  // digitalWrite(PUMP_PIN, turnOn ? HIGH : LOW);

  Serial.println("💧 Bomba " + String(turnOn ? "LIGADA" : "DESLIGADA"));

  if (turnOn)
  {
    irrigationStartTime = millis();
    irrigationActive = true;
  }
  else
  {
    irrigationActive = false;
  }
}

void offlineIrrigationLogic()
{
  if (!offlineMode)
    return;

  // Verificar irrigação a cada intervalo definido
  if (millis() - lastIrrigationCheck >= irrigationInterval)
  {
    lastIrrigationCheck = millis();

    soilMoisture = readSoilMoisture();
    Serial.println("🌱 Umidade do solo: " + String(soilMoisture) + "%");

    // Lógica de irrigação offline
    if (soilMoisture < DEFAULT_MOISTURE_THRESHOLD && !irrigationActive)
    {
      Serial.println("🚨 Solo seco detectado - iniciando irrigação offline");
      controlPump(true);
    }
    else if (irrigationActive && (millis() - irrigationStartTime >= DEFAULT_IRRIGATION_DURATION))
    {
      Serial.println("⏰ Tempo de irrigação concluído - desligando bomba");
      controlPump(false);
    }
    else if (soilMoisture >= DEFAULT_MOISTURE_THRESHOLD + 10 && irrigationActive)
    {
      Serial.println("✅ Solo adequadamente hidratado - desligando bomba");
      controlPump(false);
    }
  }
}

// Endpoint básico para status
void handleStatus()
{
  DynamicJsonDocument doc(512);

  doc["deviceId"] = WiFi.macAddress();
  doc["mode"] = offlineMode ? "offline" : "online";
  doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  doc["wifi"]["ip"] = WiFi.localIP().toString();
  doc["pump"]["status"] = pumpStatus ? "on" : "off";
  doc["sensor"]["soilMoisture"] = soilMoisture;
  doc["system"]["uptime"] = millis() / 1000;
  doc["system"]["reconnectAttempts"] = reconnectAttempts;

  String response;
  serializeJson(doc, response);

  server.send(200, "application/json", response);
}

// Endpoint para forçar reconexão
void handleReconnect()
{
  if (server.method() == HTTP_POST)
  {
    Serial.println("🔄 Reconexão forçada via API");
    reconnectAttempts = 0;
    offlineMode = false;

    DynamicJsonDocument response(256);
    response["success"] = true;
    response["message"] = "Tentativa de reconexão iniciada";

    String responseStr;
    serializeJson(response, responseStr);
    server.send(200, "application/json", responseStr);

    // Tentar reconectar
    if (WiFi.status() == WL_CONNECTED)
    {
      sendDeviceRegistration();
    }
  }
}

// Endpoint para reset WiFi
void handleResetWiFi()
{
  if (server.method() == HTTP_POST)
  {
    String body = server.arg("plain");
    DynamicJsonDocument doc(256);

    if (deserializeJson(doc, body) == DeserializationError::Ok)
    {
      if (doc.containsKey("password"))
      {
        String password = doc["password"];

        if (password == "verdea2024")
        {
          DynamicJsonDocument response(256);
          response["success"] = true;
          response["message"] = "WiFi será resetado em 3 segundos";
          response["apName"] = "VERDEA-SETUP";
          response["apPassword"] = "verdea123";

          String responseStr;
          serializeJson(response, responseStr);
          server.send(200, "application/json", responseStr);

          Serial.println("🔄 Reset de WiFi autorizado");
          delay(3000);
          wifiManager.resetSettings();
          ESP.restart();
        }
        else
        {
          server.send(401, "application/json", "{\"error\": \"Senha incorreta\"}");
        }
      }
    }
  }
}

void setup()
{
  Serial.begin(921600);
  delay(2000);

  Serial.println("🌱 =================================");
  Serial.println("🌱 Sistema de Irrigação Inteligente");
  Serial.println("🌱 Verdea - Modo Híbrido");
  Serial.println("🌱 =================================\n");

  // Configurar WiFiManager com timeout
  wifiManager.setConfigPortalTimeout(300); // 5 minutos para configurar
  wifiManager.setConnectTimeout(30);       // 30 segundos para conectar

  wifiManager.setAPCallback([](WiFiManager *myWiFiManager)
                            {
    Serial.println("📡 =================================");
    Serial.println("📡 MODO DE CONFIGURAÇÃO ATIVADO");
    Serial.println("📡 =================================");
    Serial.println("📡 Rede: VERDEA-SETUP");
    Serial.println("📡 Senha: verdea123");
    Serial.println("📡 IP: " + WiFi.softAPIP().toString());
    Serial.println("📡 =================================");
    Serial.println("📡 Acesse: http://" + WiFi.softAPIP().toString());
    Serial.println("📡 Tempo limite: 5 minutos");
    Serial.println("📡 =================================\n"); });

  Serial.println("🔄 Tentando conectar ao WiFi...");

  if (!wifiManager.autoConnect("VERDEA-SETUP", "verdea123"))
  {
    Serial.println("❌ Falha na conexão WiFi");
    Serial.println("🚨 ATIVANDO MODO OFFLINE IMEDIATAMENTE");

    offlineMode = true;
    isOnlineMode = false;
    reconnectAttempts = maxReconnectAttempts;

    Serial.println("🌱 Sistema funcionará autonomamente");
  }
  else
  {
    Serial.println("✅ Conectado ao WiFi!");
    Serial.println("📶 Rede: " + WiFi.SSID());
    Serial.println("📡 IP: " + WiFi.localIP().toString());
    Serial.println("📊 Sinal: " + String(WiFi.RSSI()) + " dBm\n");

    // Tentar registrar na API
    sendDeviceRegistration();
  }

  // Configurar endpoints mínimos
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/reconnect", HTTP_POST, handleReconnect);
  server.on("/api/reset-wifi", HTTP_POST, handleResetWiFi);

  server.begin();
  Serial.println("🌐 Servidor local iniciado");
  Serial.println("🌐 Endpoints disponíveis:");
  Serial.println("   GET  /api/status");
  Serial.println("   POST /api/reconnect");
  Serial.println("   POST /api/reset-wifi\n");

  Serial.println("🚀 Sistema iniciado com sucesso!");
  Serial.println("🚀 Modo: " + String(offlineMode ? "OFFLINE" : "ONLINE"));
  Serial.println("🚀 =================================\n");
}

void loop()
{
  server.handleClient();

  // Verificar conexão WiFi
  if (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - lastConnectionAttempt > connectionTimeout)
    {
      lastConnectionAttempt = millis();
      Serial.println("📶 WiFi desconectado - tentando reconectar...");
      WiFi.reconnect();

      // Se não conseguir reconectar, ativar modo offline
      if (reconnectAttempts < maxReconnectAttempts)
      {
        checkOfflineMode();
      }
    }
  }
  else
  {
    // WiFi conectado
    if (offlineMode && reconnectAttempts >= maxReconnectAttempts)
    {
      Serial.println("🔄 WiFi reconectado - tentando voltar ao modo online...");
      reconnectAttempts = 0;
      sendDeviceRegistration();
    }

    // Enviar heartbeat a cada 60 segundos se estiver online
    if (isOnlineMode && millis() - lastHeartbeat > 60000)
    {
      sendHeartbeat();
    }

    // Verificar se precisa reregistrar
    if (!alreadyRegistered && !offlineMode)
    {
      sendDeviceRegistration();
    }
  }

  // Lógica de irrigação offline
  offlineIrrigationLogic();

  // Status periódico
  static unsigned long lastStatusLog = 0;
  if (millis() - lastStatusLog > 300000) // A cada 5 minutos
  {
    lastStatusLog = millis();
    Serial.println("📊 Status: " + String(offlineMode ? "OFFLINE" : "ONLINE") +
                   " | WiFi: " + String(WiFi.status() == WL_CONNECTED ? "OK" : "FALHA") +
                   " | Uptime: " + String(millis() / 1000) + "s");
  }

  delay(1000);
}