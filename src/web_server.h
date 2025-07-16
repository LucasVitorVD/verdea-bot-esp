#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "wifi_manager.h"
#include "api_manager.h"
#include "irrigation_controller.h"
#include "auth_utils.h"


// Referência ao servidor global
extern ESP8266WebServer server;

// Declarações das funções
void initWebServer();
void handleStatus();
void handleReconnect();
void handleResetWiFi();
void handleStatusLog();

// Implementações
void initWebServer()
{
  // Configurar endpoints
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/reconnect", HTTP_POST, handleReconnect);
  server.on("/api/reset-wifi", HTTP_POST, handleResetWiFi);

  server.begin();
  Serial.println("🌐 Servidor local iniciado");
  Serial.println("🌐 Endpoints disponíveis:");
  Serial.println("   GET  /api/status");
  Serial.println("   POST /api/reconnect");
  Serial.println("   POST /api/reset-wifi\n");
}

void handleStatus()
{
  DynamicJsonDocument doc(512);

  doc["deviceId"] = WiFi.macAddress();
  doc["mode"] = getOfflineMode() ? "offline" : "online";
  doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  doc["wifi"]["ip"] = WiFi.localIP().toString();
  doc["pump"]["status"] = getPumpStatus() ? "on" : "off";
  doc["sensor"]["soilMoisture"] = getSoilMoisture();
  doc["system"]["uptime"] = millis() / 1000;
  doc["system"]["reconnectAttempts"] = reconnectAttempts;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleReconnect()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "application/json", "{\"error\": \"Método não permitido\"}");
    return;
  }

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
    tryReconnectAPI();
  }
  else
  {
    Serial.println("📶 WiFi desconectado - tentando reconectar...");
  }
}

void handleResetWiFi() {
  if (server.method() == HTTP_POST) {
    if (!checkBasicAuth(server)) return;

    DynamicJsonDocument response(256);
    response["success"] = true;
    response["message"] = "WiFi será resetado em 3 segundos";
    response["apName"] = WIFI_AP_NAME;
    response["apPassword"] = WIFI_AP_PASSWORD;

    String responseStr;
    serializeJson(response, responseStr);
    server.send(200, "application/json", responseStr);

    Serial.println("🔐 Reset autorizado via autenticação BASIC");
    delay(3000);
    resetWiFiSettings();
    ESP.restart();
  }
}

void handleStatusLog()
{
  static unsigned long lastStatusLog = 0;

  if (millis() - lastStatusLog > STATUS_LOG_INTERVAL)
  {
    lastStatusLog = millis();
    Serial.println("📊 STATUS LOG:");
    Serial.println("   🌐 Modo: " + String(getOfflineMode() ? "OFFLINE" : "ONLINE"));
    Serial.println("   📶 WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado"));
    Serial.println("   ⏱️ Uptime: " + String(millis() / 1000) + " segundos\n");
  }
}

#endif