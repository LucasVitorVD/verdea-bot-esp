#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "wifi_manager.h"
#include "irrigation_controller.h"

// Tópicos base
const char *topic_commands = "verdea/commands";
const char *topic_register = "verdea/device/register";

// Instâncias globais para o cliente MQTT
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// Declarações das funções
void initMQTT();
void handleMQTT();
void reconnectMQTT();
void publishStatus(String status);
void mqttCallback(char *topic, byte *payload, unsigned int length);
void publishRegistrationMessage();
String getDeviceMacClean();
String getDeviceTopicStatus();
String getDeviceTopicCommands();
String buildStatusPayload(String status);

String getDeviceTopicStatus()
{
  return "verdea/status/" + getDeviceMacClean();
}

String getDeviceTopicCommands()
{
  return "verdea/commands/" + getDeviceMacClean();
}

String buildStatusPayload(String status)
{
  DynamicJsonDocument doc(128);
  doc["macAddress"] = WiFi.macAddress();
  doc["status"] = status;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

void initMQTT()
{
  // Configurar cliente SSL para desenvolvimento
  espClient.setInsecure();

  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("📩 Mensagem recebida em ");
  Serial.print(topic);
  Serial.print(": ");

  String msg;
  for (unsigned int i = 0; i < length; i++)
  {
    msg += (char)payload[i];
  }
  Serial.println(msg);

  String deviceTopicCommands = getDeviceTopicCommands();

  if (String(topic) == topic_commands || String(topic) == deviceTopicCommands)
  {
    // Tenta interpretar como JSON
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, msg);

    if (!error)
    {
      // Se o payload for JSON válido, interpretamos como config de planta
      String mode = doc["mode"] | "AUTO";
      String wateringTime = doc["wateringTime"] | "";
      String wateringFrequency = doc["wateringFrequency"] | "";
      wateringFrequency.toLowerCase();
      int idealSoilMoisture = doc["idealSoilMoisture"].is<int>()
                                  ? doc["idealSoilMoisture"].as<int>()
                                  : (int)doc["idealSoilMoisture"].as<double>();

      // ✅ Converte "HH:mm" em hour/minute
      int hour = 0, minute = 0;
      if (wateringTime.length() >= 4)
      {
        int sepIndex = wateringTime.indexOf(':');
        if (sepIndex > 0)
        {
          hour = wateringTime.substring(0, sepIndex).toInt();
          minute = wateringTime.substring(sepIndex + 1).toInt();
        }
      }

      setIrrigationConfig(mode, hour, minute, wateringFrequency, idealSoilMoisture);

      // ✅ ADICIONAR: Debug da configuração recebida
      Serial.println("🔧 Configuração recebida via MQTT:");
      Serial.println("   Mode: " + mode);
      Serial.println("   Hour: " + String(hour));
      Serial.println("   Minute: " + String(minute));
      Serial.println("   Frequency: " + wateringFrequency);
      Serial.print("   Ideal Moisture: ");
      Serial.println(idealSoilMoisture);

      // Checa se é comando de DELETE_PLANT
      if (doc.containsKey("command") && String(doc["command"]) == "DELETE_PLANT")
      {
        resetIrrigationConfig();
        controlPump(false, "Modo AUTO (planta deletada)");
        Serial.println("🚨 Planta deletada - ESP resetado para AUTO");
      }
    }
    else
    {
      // Caso não seja JSON → trata como comando simples
      if (msg == "ON")
      {
        controlPump(true, "Comando manual ON.");
        Serial.println("💧 Comando de LIGAR bomba recebido.");
      }
      else if (msg == "RESET_WIFI")
      {
        Serial.println("🚨 Comando de RESET_WIFI recebido via MQTT!");
        delay(1000);
        resetWiFiSettings();
        ESP.restart();
      }
      else if (msg == "DELETE_PLANT")
      {
        resetIrrigationConfig();
      }
      else
      {
        Serial.println("⚠️ Payload ignorado (não é JSON nem comando conhecido).");
      }
    }
  }
}

void reconnectMQTT()
{
  int attempts = 0;
  const int maxAttempts = 5;

  while (!mqttClient.connected() && WiFi.status() == WL_CONNECTED && attempts < maxAttempts)
  {
    Serial.print("🔄 Tentativa ");
    Serial.print(attempts + 1);
    Serial.print("/");
    Serial.print(maxAttempts);
    Serial.print(" - Conectando ao broker MQTT...");

    String clientId = "irrigacao-verdea-" + getDeviceMacClean();
    String willTopic = getDeviceTopicStatus();
    String willPayload = buildStatusPayload("offline"); // <-- CORREÇÃO: Usar função para criar JSON

    if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password, willTopic.c_str(), 1, true, willPayload.c_str()))
    {
      Serial.println("✅ Conectado!");

      // ✅ ADICIONAR: Log dos tópicos que está se inscrevendo
      String deviceCommandsTopic = getDeviceTopicCommands();
      Serial.print("🔗 Inscrevendo no tópico específico: ");
      Serial.println(deviceCommandsTopic);
      mqttClient.subscribe(deviceCommandsTopic.c_str());

      Serial.print("🔗 Inscrevendo no tópico geral: ");
      Serial.println(topic_commands);
      mqttClient.subscribe(topic_commands);

      Serial.println("🔗 Inscrições realizadas com sucesso!");

      // Publicar mensagem de registro apenas uma vez por sessão
      publishRegistrationMessage();

      // Publicar status ONLINE após a conexão bem-sucedida
      publishStatus("online");
      return;
    }
    else
    {
      Serial.print("❌ Falha, rc=");
      Serial.print(mqttClient.state());
      Serial.print(" (");

      switch (mqttClient.state())
      {
      case -4:
        Serial.print("MQTT_CONNECTION_TIMEOUT");
        break;
      case -3:
        Serial.print("MQTT_CONNECTION_LOST");
        break;
      case -2:
        Serial.print("MQTT_CONNECT_FAILED");
        break;
      case -1:
        Serial.print("MQTT_DISCONNECTED");
        break;
      case 1:
        Serial.print("MQTT_CONNECT_BAD_PROTOCOL");
        break;
      case 2:
        Serial.print("MQTT_CONNECT_BAD_CLIENT_ID");
        break;
      case 3:
        Serial.print("MQTT_CONNECT_UNAVAILABLE");
        break;
      case 4:
        Serial.print("MQTT_CONNECT_BAD_CREDENTIALS");
        break;
      case 5:
        Serial.print("MQTT_CONNECT_UNAUTHORIZED");
        break;
      default:
        Serial.print("UNKNOWN");
        break;
      }

      Serial.println(")");
      attempts++;

      if (attempts < maxAttempts)
      {
        Serial.println("⏱️ Aguardando 5 segundos para nova tentativa...");
        delay(5000);
      }
    }
  }

  if (!mqttClient.connected())
  {
    Serial.println("🚨 Máximo de tentativas de conexão MQTT excedido!");
  }
}

void publishRegistrationMessage()
{
  if (mqttClient.connected())
  {
    String mac = WiFi.macAddress();
    String ip = WiFi.localIP().toString();
    String macClean = getDeviceMacClean();
    String macSuffix = macClean.substring(6);
    String deviceName = "irrigacao-verdea-" + macSuffix;

    // Cria um objeto JSON com todos os dados
    DynamicJsonDocument doc(256);
    doc["name"] = deviceName;
    doc["macAddress"] = mac;
    doc["currentIp"] = ip;

    String registrationPayload;
    serializeJson(doc, registrationPayload);

    mqttClient.publish(topic_register, registrationPayload.c_str());
    Serial.println("📤 Mensagem de registro com dados completos publicada.");
  }
}

void handleMQTT()
{
  if (!mqttClient.connected())
  {
    reconnectMQTT();
  }
  else
  {
    mqttClient.loop();
  }
}

void publishStatus(String status)
{
  if (mqttClient.connected())
  {
    String deviceTopicStatus = getDeviceTopicStatus();
    String payload = buildStatusPayload(status);

    // Publica status no tópico específico do dispositivo com retained = true
    bool result = mqttClient.publish(deviceTopicStatus.c_str(), payload.c_str(), true);

    if (result)
    {
      Serial.print("📤 Status '");
      Serial.print(status);
      Serial.println("' publicado com sucesso");
    }
    else
    {
      Serial.println("❌ Falha ao publicar status");
    }
  }
  else
  {
    Serial.println("⚠️ MQTT não conectado - não foi possível publicar status");
  }
}

#endif