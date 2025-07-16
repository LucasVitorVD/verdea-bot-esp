#ifndef API_MANAGER_H
#define API_MANAGER_H

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "wifi_manager.h"

// Variáveis globais para API
extern String lastSentMac;
extern bool alreadyRegistered;
extern unsigned long lastHeartbeat;

// Declarações das funções
void initAPI();
void handleAPIConnection();
void sendDeviceRegistration();
void sendHeartbeat();
void tryReconnectAPI();
bool isAPIConnected();
void switchToOnlineMode();

// Implementações
void initAPI() {
  lastSentMac = "";
  alreadyRegistered = false;
  lastHeartbeat = millis();
  
  // Tentar registrar dispositivo
  sendDeviceRegistration();
}

void handleAPIConnection() {
  if (!getOnlineMode()) return;

  // Enviar heartbeat a cada intervalo definido
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
  }

  // Verificar se precisa reregistrar
  if (!alreadyRegistered && !getOfflineMode()) {
    sendDeviceRegistration();
  }
}

void sendDeviceRegistration() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi não conectado - não é possível registrar dispositivo");
    return;
  }

  HTTPClient http;
  WiFiClient client;

  http.setTimeout(HTTP_TIMEOUT);
  http.begin(client, String(API_URL));
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

  if (httpCode >= 200 && httpCode < 300) {
    Serial.println("✅ Dispositivo registrado na API!");
    Serial.print("Código HTTP: ");
    Serial.println(httpCode);

    alreadyRegistered = true;
    lastSentMac = mac;
    switchToOnlineMode();

    Serial.println("🌐 MODO ONLINE ativado");
  } else if (httpCode == 409 || httpCode == 422) {
    Serial.println("⚠️ Dispositivo já registrado");
    alreadyRegistered = true;
    switchToOnlineMode();
  } else {
    Serial.print("❌ Erro na requisição HTTP: ");
    Serial.println(httpCode);
    Serial.println("Resposta: " + http.getString());

    // Se falhar, considerar modo offline
    checkOfflineMode();
  }

  http.end();
}

void sendHeartbeat() {
  /* if (WiFi.status() != WL_CONNECTED || !getOnlineMode()) return;

  HTTPClient http;
  WiFiClient client;

  http.setTimeout(HEARTBEAT_HTTP_TIMEOUT);
  http.begin(client, String(HEARTBEAT_URL));
  http.addHeader("Content-Type", "application/json");

  String mac = WiFi.macAddress();
  String payload = "{\"macAddress\": \"" + mac + "\", \"status\": \"ONLINE\", \"timestamp\": " + String(millis()) + "}";

  int httpCode = http.POST(payload);

  if (httpCode > 0 && httpCode < 400) {
    Serial.println("💓 Heartbeat enviado com sucesso");
    lastHeartbeat = millis();
  } else {
    Serial.println("❌ Falha no heartbeat - HTTP: " + String(httpCode));
    
    // Se falhar heartbeat, verificar se deve entrar em modo offline
    if (millis() - lastHeartbeat > HEARTBEAT_TIMEOUT) {
      Serial.println("🚨 Sem comunicação com servidor - ativando modo offline");
      checkOfflineMode();
    }
  }

  http.end(); */
}

void tryReconnectAPI() {
  reconnectAttempts = 0;
  offlineMode = false;
  sendDeviceRegistration();
}

bool isAPIConnected() {
  return alreadyRegistered && getOnlineMode();
}

void switchToOnlineMode() {
  offlineMode = false;
  isOnlineMode = true;
  reconnectAttempts = 0;
}

// Variáveis globais
String lastSentMac = "";
bool alreadyRegistered = false;
unsigned long lastHeartbeat = 0;

#endif