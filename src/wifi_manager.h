#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "config.h"

// Definição das variáveis globais
WiFiManager wifiManager;
unsigned long lastConnectionAttempt = 0;
int reconnectAttempts = 0;
bool offlineMode = false;
bool isOnlineMode = false;

// Variáveis internas para controle
bool isReconnecting = false;
String userEmail = "";
bool isConfigured = false;

// Constantes
const char* configFilePath = "/config.txt";
const char* api_endpoint = "http://192.168.15.115:8080/api/device/send-mac";

// Parâmetro personalizado para o e-mail
WiFiManagerParameter customEmail("userEmail", "E-mail cadastrado na plataforma Verdea", "", 50, "required");

// Declarações das funções
void initWiFi();
void handleWiFiConnection();
void checkOfflineMode();
bool getOfflineMode();
bool getOnlineMode();
void resetWiFiSettings();
void saveConfig();
void loadConfig();
String getDeviceMacClean();
bool sendDeviceToBackend();

// ================= IMPLEMENTAÇÕES DAS FUNÇÕES =================

void saveConfig() {
  if (!SPIFFS.begin()) {
    Serial.println("❌ Falha ao inicializar SPIFFS");
    return;
  }
  
  File configFile = SPIFFS.open(configFilePath, "w");
  if (!configFile) {
    Serial.println("❌ Erro ao abrir arquivo para escrita.");
    SPIFFS.end();
    return;
  }
  
  configFile.print("configured");
  configFile.close();
  SPIFFS.end();
  Serial.println("✅ Estado de configuração salvo.");
}

void loadConfig() {
  if (!SPIFFS.begin()) {
    Serial.println("❌ Falha ao inicializar SPIFFS");
    return;
  }
  
  if (SPIFFS.exists(configFilePath)) {
    File configFile = SPIFFS.open(configFilePath, "r");
    if (configFile) {
      String content = configFile.readString();
      configFile.close();
      
      if (content.indexOf("configured") != -1) {
        isConfigured = true;
        Serial.println("✅ Estado de configuração carregado.");
      }
    }
  }
  SPIFFS.end();
}

String getDeviceMacClean() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return mac;
}

bool sendDeviceToBackend() {
  if (userEmail.length() == 0) {
    Serial.println("⚠️ Nenhum e-mail fornecido.");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  http.begin(client, api_endpoint);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000); // 10 segundos timeout

  DynamicJsonDocument doc(256);
  doc["email"] = userEmail;
  doc["deviceName"] = "irrigacao-verdea-" + getDeviceMacClean();
  doc["macAddress"] = WiFi.macAddress();

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  Serial.println("📤 Enviando payload: " + jsonPayload);

  int httpResponseCode = http.POST(jsonPayload);
  String response = http.getString();
  
  http.end();

  if (httpResponseCode == 200) {
    Serial.println("✅ Resposta da API (" + String(httpResponseCode) + "): " + response);
    return true;
  } else {
    Serial.println("❌ Erro na requisição HTTP: " + String(httpResponseCode));
    Serial.println("Resposta: " + response);
    return false;
  }
}

void initWiFi() {
  Serial.println("🔄 Inicializando WiFi Manager...");
  
  // Carrega configuração existente
  loadConfig();
  
  if (isConfigured) {
    Serial.println("✅ Configuração já existente. Tentando conectar...");
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    
    // Aguarda conexão por até 30 segundos
    int timeout = 30;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
      delay(1000);
      Serial.print(".");
      timeout--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Conectado ao WiFi!");
      Serial.println("📶 Rede: " + WiFi.SSID());
      Serial.println("📍 IP: " + WiFi.localIP().toString());
      isOnlineMode = true;
      offlineMode = false;
      return;
    } else {
      Serial.println("\n❌ Falha na conexão automática. Iniciando configuração...");
      isConfigured = false; // Reset para forçar nova configuração
    }
  }
  
  // Configurar WiFiManager
  wifiManager.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT);
  wifiManager.addParameter(&customEmail);

  // Callback para quando entra em modo AP
  wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial.println("📡 =================================");
    Serial.println("📡 MODO DE CONFIGURAÇÃO ATIVADO");
    Serial.println("📡 =================================");
    Serial.println("📡 Rede: " + String(WIFI_AP_NAME));
    Serial.println("📡 Senha: " + String(WIFI_AP_PASSWORD));
    Serial.println("📡 IP: " + WiFi.softAPIP().toString());
    Serial.println("📡 =================================");
    Serial.println("📡 Acesse: http://" + WiFi.softAPIP().toString());
    Serial.println("📡 Tempo limite: " + String(WIFI_CONFIG_TIMEOUT/60000) + " minutos");
    Serial.println("📡 =================================\n");
  });

  // Callback para quando salva configuração
  wifiManager.setSaveConfigCallback([]() {
    Serial.println("✅ Configuração WiFi salva!");
    userEmail = customEmail.getValue();
    Serial.println("📧 E-mail recebido: " + userEmail);
  });

  Serial.println("🔄 Iniciando portal de configuração...");
  
  if (wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD)) {
    Serial.println("✅ Conectado ao WiFi!");
    Serial.println("📶 Rede: " + WiFi.SSID());
    Serial.println("📍 IP: " + WiFi.localIP().toString());
    
    userEmail = customEmail.getValue();
    
    if (userEmail.length() > 0) {
      Serial.println("📤 Enviando dados para o backend...");
      
      if (sendDeviceToBackend()) {
        Serial.println("✅ Dispositivo vinculado com sucesso!");
        saveConfig();
        isOnlineMode = true;
        offlineMode = false;
        
        Serial.println("✅ Configuração concluída! Reiniciando em 3 segundos...");
        delay(3000);
        ESP.restart();
      } else {
        Serial.println("❌ Falha na vinculação com backend.");
        Serial.println("🔄 Resetando configurações para nova tentativa...");
        resetWiFiSettings();
        delay(3000);
        ESP.restart();
      }
    } else {
      Serial.println("⚠️ E-mail não fornecido. Resetando configurações...");
      resetWiFiSettings();
      delay(3000);
      ESP.restart();
    }
  } else {
    Serial.println("❌ Timeout na configuração WiFi");
    Serial.println("🚨 ATIVANDO MODO OFFLINE");
    
    offlineMode = true;
    isOnlineMode = false;
    reconnectAttempts = MAX_RECONNECT_ATTEMPTS;
    
    Serial.println("🌱 Sistema funcionará em modo autônomo");
  }
}

void handleWiFiConnection() {
  // Se está em modo offline, não tenta reconectar
  if (offlineMode) {
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    if (!isReconnecting && (millis() - lastConnectionAttempt) > CONNECTION_TIMEOUT) {
      isReconnecting = true;
      lastConnectionAttempt = millis();
      reconnectAttempts++;
      
      Serial.println("📶 WiFi desconectado - tentativa " + String(reconnectAttempts) + 
                     "/" + String(MAX_RECONNECT_ATTEMPTS));
      
      if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        WiFi.reconnect();
      } else {
        Serial.println("🚨 Máximo de tentativas atingido - ATIVANDO MODO OFFLINE");
        offlineMode = true;
        isOnlineMode = false;
        reconnectAttempts = 0;
        isReconnecting = false;
        Serial.println("🌱 Sistema funcionará em modo autônomo");
      }
    }
  } else {
    if (isReconnecting) {
      Serial.println("✅ Reconexão WiFi bem-sucedida");
      isReconnecting = false;
      reconnectAttempts = 0;
    }
    
    if (offlineMode) {
      offlineMode = false;
      isOnlineMode = true;
      Serial.println("🔄 WiFi reconectado - Sistema ONLINE");
    }
  }
}

void checkOfflineMode() {
  if (!offlineMode) {
    reconnectAttempts++;
    Serial.println("🔄 Verificação offline - tentativa: " + String(reconnectAttempts) + 
                   "/" + String(MAX_RECONNECT_ATTEMPTS));

    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
      Serial.println("🚨 Máximo de tentativas atingido - ATIVANDO MODO OFFLINE");
      offlineMode = true;
      isOnlineMode = false;
      reconnectAttempts = 0;
      Serial.println("🌱 Sistema funcionará em modo autônomo");
    }
  }
}

// Funções getter
bool getOfflineMode() { 
  return offlineMode; 
}

bool getOnlineMode() { 
  return isOnlineMode; 
}

// Função para reset das configurações WiFi
void resetWiFiSettings() { 
  Serial.println("🔄 Resetando configurações WiFi...");
  wifiManager.resetSettings();
  
  // Remove arquivo de configuração
  if (SPIFFS.begin()) {
    if (SPIFFS.exists(configFilePath)) {
      SPIFFS.remove(configFilePath);
      Serial.println("✅ Arquivo de configuração removido.");
    }
    SPIFFS.end();
  }
  
  isConfigured = false;
  Serial.println("✅ Configurações resetadas com sucesso!");
}

#endif