#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// Incluir os módulos
#include "config.h"
#include "wifi_manager.h"
#include "irrigation_controller.h"
#include "web_server.h"
#include "mqtt_manager.h"

// Instâncias globais
ESP8266WebServer server(80);
WiFiManager wifiManager;

void setup()
{
  Serial.begin(921600);
  delay(2000);

  Serial.println("🌱 =================================");
  Serial.println("🌱 Sistema de Irrigação Inteligente");
  Serial.println("🌱 Verdea - Modo Híbrido");
  Serial.println("🌱 =================================\n");

  // Inicializar WiFi
  initWiFi();

  // Inicializar API se conectado
  if (WiFi.status() == WL_CONNECTED)
  {
    // initAPI();

    // Inicializar MQTT
    initMQTT();
  }

  // Inicializar sistema de irrigação
  initIrrigation();

  // Inicializar servidor web
  initWebServer();

  Serial.println("🚀 Sistema iniciado com sucesso!");
  Serial.println("🚀 Modo: " + String(getOfflineMode() ? "OFFLINE" : "ONLINE"));
  Serial.println("🚀 =================================\n");
}

void loop()
{
  // Processar requisições web
  server.handleClient();

  // Gerenciar conectividade WiFi
  handleWiFiConnection();

  // Processar MQTT
  handleMQTT();

  // Controlar sistema de irrigação
  handleIrrigation();

  // Log de status periódico
  handleStatusLog();

  delay(1000);
}