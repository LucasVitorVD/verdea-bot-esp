#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Definição das variáveis globais
WiFiManager wifiManager;
unsigned long lastConnectionAttempt = 0;
int reconnectAttempts = 0;
bool offlineMode = false;
bool isOnlineMode = false;
bool isReconnecting = false;
String userEmail = "";

// Constantes
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
String getDeviceMacClean();
bool sendDeviceToBackend();

// ================= IMPLEMENTAÇÕES DAS FUNÇÕES =================

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
  
  // Configurar WiFiManager
  wifiManager.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT);
  wifiManager.addParameter(&customEmail);

  // Mensagem de instrução personalizada
  String customHtml = "<div style='margin: 20px 0; padding: 15px; background-color: #e3f2fd; border-radius: 8px; border-left: 4px solid #2196f3; box-shadow: 0 2px 4px rgba(0,0,0,0.1);'>";
  customHtml += "<h3 style='margin-top: 0; color: #1976d2; font-family: Arial, sans-serif;'>📧 Instruções Importantes:</h3>";
  customHtml += "<p style='margin: 10px 0; font-size: 14px;'><strong>1.</strong> Insira o e-mail <strong>cadastrado na plataforma Verdea</strong></p>";
  customHtml += "<p style='margin: 10px 0; font-size: 14px;'><strong>2.</strong> Após conectar, <strong>verifique se recebeu confirmação no e-mail</strong></p>";
  customHtml += "<p style='margin: 10px 0; font-size: 14px;'><strong>3.</strong> Se não receber confirmação, reconecte-se à rede <strong style='color: #d32f2f;'>" + String(WIFI_AP_NAME) + "</strong> e reconfigure</p>";
  customHtml += "<p style='margin: 15px 0 0 0; padding: 10px; background-color: #fff3e0; border-radius: 4px; font-size: 13px; color: #ef6c00;'>";
  customHtml += "⚠️ <strong>Importante:</strong> Se o e-mail não for encontrado na plataforma, as configurações serão resetadas automaticamente.</p>";
  customHtml += "</div>";

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

  // Define a mensagem customizada
  wifiManager.setCustomHeadElement(customHtml.c_str());

  Serial.println("🔄 Iniciando conexão WiFi...");
  
  if (wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD)) {
    Serial.println("✅ Conectado ao WiFi!");
    Serial.println("📶 Rede: " + WiFi.SSID());
    Serial.println("📍 IP: " + WiFi.localIP().toString());
    
    userEmail = customEmail.getValue();
    
    if (userEmail.length() > 0) {
      Serial.println("📤 Enviando dados para o backend...");
      
      if (sendDeviceToBackend()) {
        Serial.println("✅ Dispositivo vinculado com sucesso!");
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
      // Se conectou mas não tem e-mail (conexão automática), funciona normalmente
      Serial.println("ℹ️ Conectado automaticamente - funcionando normalmente");
      isOnlineMode = true;
      offlineMode = false;
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
  wifiManager.resetSettings(); // Isso limpa TUDO que o WiFiManager salvou
  userEmail = "";
  Serial.println("✅ Configurações resetadas com sucesso!");
}

#endif