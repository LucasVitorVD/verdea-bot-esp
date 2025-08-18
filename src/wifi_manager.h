#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include "config.h"

// Variáveis globais para WiFi
extern WiFiManager wifiManager;
extern unsigned long lastConnectionAttempt;
extern int reconnectAttempts;
extern bool offlineMode;
extern bool isOnlineMode;

// Declarações das funções
void initWiFi();
void handleWiFiConnection();
void checkOfflineMode();
bool getOfflineMode();
bool getOnlineMode();
void resetWiFiSettings();

// Variável interna para controle de reconexão
bool isReconnecting = false;

// Implementações
void initWiFi()
{
  // Configurar WiFiManager com timeout
  wifiManager.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT);

  wifiManager.setAPCallback([](WiFiManager *myWiFiManager)
                            {
    Serial.println("📡 =================================");
    Serial.println("📡 MODO DE CONFIGURAÇÃO ATIVADO");
    Serial.println("📡 =================================");
    Serial.println("📡 Rede: " + String(WIFI_AP_NAME));
    Serial.println("📡 Senha: " + String(WIFI_AP_PASSWORD));
    Serial.println("📡 IP: " + WiFi.softAPIP().toString());
    Serial.println("📡 =================================");
    Serial.println("📡 Acesse: http://" + WiFi.softAPIP().toString());
    Serial.println("📡 Tempo limite: 5 minutos");
    Serial.println("📡 =================================\n"); });

  Serial.println("🔄 Tentando conectar ao WiFi...");

  if (!wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD))
  {
    Serial.println("❌ Falha na conexão WiFi");
    Serial.println("🚨 ATIVANDO MODO OFFLINE IMEDIATAMENTE");

    offlineMode = true;
    isOnlineMode = false;
    reconnectAttempts = MAX_RECONNECT_ATTEMPTS;

    Serial.println("🌱 Sistema funcionará autonomamente");
  }
  else
  {
    Serial.println("✅ Conectado ao WiFi!");
    Serial.println("📶 Rede: " + WiFi.SSID());

    offlineMode = false;
    isOnlineMode = true;
    reconnectAttempts = 0;
  }
}

void handleWiFiConnection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    if (!isReconnecting && millis() - lastConnectionAttempt > CONNECTION_TIMEOUT)
    {
      isReconnecting = true;
      lastConnectionAttempt = millis();
      Serial.println("📶 WiFi desconectado - tentando reconectar...");
      WiFi.reconnect();
    }

    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS)
    {
      reconnectAttempts = 0;
      
      Serial.println("🚨 Máximo de tentativas atingido - entrando em modo AP para reconfiguração");

      wifiManager.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);

      bool configResult = wifiManager.startConfigPortal(WIFI_AP_NAME, WIFI_AP_PASSWORD);

      if (configResult)
      {
        Serial.println("✅ Configuração WiFi concluída com sucesso!");
      }
      else
      {
        Serial.println("⏰ Timeout do modo AP expirado - tentando reconectar...");
      }

      // Após sair do modo AP, tenta conectar normalmente
      if (WiFi.status() != WL_CONNECTED)
      {
        Serial.println("🔄 Tentando conectar ao WiFi após modo AP...");
        if (!wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD))
        {
          Serial.println("❌ Falha na conexão WiFi após modo AP");
          // Entrar em modo offline só após esse segundo fracasso
          offlineMode = true;
          isOnlineMode = false;
          reconnectAttempts = 0;
          Serial.println("🌱 Entrando em modo OFFLINE");
        }
        else
        {
          Serial.println("✅ Conectado ao WiFi após modo AP");
          offlineMode = false;
          isOnlineMode = true;
          reconnectAttempts = 0;
        }
      }

      isReconnecting = false; // Reset flag
    }
  }
  else // WiFi conectado
  {
    if (isReconnecting)
    {
      Serial.println("✅ Reconexão WiFi bem-sucedida");
      isReconnecting = false;
    }

    if (offlineMode)
    {
      offlineMode = false;
      reconnectAttempts = 0;
      isOnlineMode = true;
      Serial.println("🔄 WiFi reconectado - Saiu do modo OFFLINE");
    }
  }
}

void checkOfflineMode()
{
  reconnectAttempts++;
  Serial.println("🔄 Tentativa de reconexão: " + String(reconnectAttempts) + "/" + String(MAX_RECONNECT_ATTEMPTS));

  if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS)
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

bool getOfflineMode()
{
  return offlineMode;
}

bool getOnlineMode()
{
  return isOnlineMode;
}

void resetWiFiSettings()
{
  wifiManager.resetSettings();
}

// Variáveis globais
unsigned long lastConnectionAttempt = 0;
int reconnectAttempts = 0;
bool offlineMode = false;
bool isOnlineMode = false;

#endif