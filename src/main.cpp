#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <time.h>
#include <TimeLib.h>
#include "time_utils.h"

// Incluir os módulos
#include "config.h"
#include "wifi_manager.h"
#include "irrigation_controller.h"
#include "web_server.h"
#include "mqtt_manager.h"

// Instâncias globais
ESP8266WebServer server(80);

void setup()
{
  Serial.begin(921600);
  delay(2000);

  Serial.println("🌱 =================================");
  Serial.println("🌱 Sistema de Irrigação Inteligente Verdea");
  Serial.println("🌱 =================================\n");

  // Inicializar WiFi
  initWiFi();

  // Inicializar MQTT se conectado
  if (WiFi.status() == WL_CONNECTED)
  {
    // initAPI();

    setupTime();

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

  // ✅ Sincronizar horário periodicamente se online
  static unsigned long lastTimeSync = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastTimeSync > 3600000) // A cada 1 hora
  {
    setupTime();
    lastTimeSync = millis();
  }

  // Processar MQTT
  handleMQTT();

  // Controlar sistema de irrigação
  handleIrrigation();

  // Gerenciar timer de irrigação
  handleIrrigationTimer();

  // Log de status periódico
  handleStatusLog();

  // ✅ Debug temporário no loop()
  static unsigned long lastTimeDebug = 0;
  if (millis() - lastTimeDebug > 30000)
  { // A cada 30 segundos
    if (isTimeSynchronized())
    {
      struct tm timeinfo;
      time_t nowTime = time(nullptr);
      localtime_r(&nowTime, &timeinfo);
      Serial.printf("🕐 DEBUG: Hora local: %02d:%02d:%02d\n",
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    lastTimeDebug = millis();
  }

  delay(1000);
}