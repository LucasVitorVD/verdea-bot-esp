#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "wifi_manager.h"
#include "irrigation_controller.h"

// Configurações do broker MQTT
const char *mqtt_broker = "702eccd77d014186bfc53d1a2d5546d8.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char *mqtt_username = "verdea";
const char *mqtt_password = "Verdea14072025";

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

String getDeviceMacClean()
{
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return mac;
}

String getDeviceTopicStatus()
{
  return "verdea/status/" + getDeviceMacClean();
}

String getDeviceTopicCommands()
{
  return "verdea/commands/" + getDeviceMacClean();
}

String buildStatusPayload(String status) {
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

  // Lógica para comandos de irrigação (tópicos gerais ou específicos do dispositivo)
  String deviceTopicCommands = getDeviceTopicCommands();

  if (String(topic) == topic_commands || String(topic) == deviceTopicCommands)
  {
    if (msg == "ON")
    {
      controlPump(true);
      Serial.println("💧 Comando de LIGAR bomba recebido.");
    }
    else if (msg == "OFF")
    {
      controlPump(false);
      Serial.println("💧 Comando de DESLIGAR bomba recebido.");
    }
    else if (msg == "RESET_WIFI")
    {
      Serial.println("🚨 Comando de RESET_WIFI recebido via MQTT!");
      delay(1000); // Dá tempo para o log ser enviado
      resetWiFiSettings();
      ESP.restart(); // Reinicia o dispositivo
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

      // Inscrever nos tópicos de comando
      mqttClient.subscribe(getDeviceTopicCommands().c_str());
      mqttClient.subscribe(topic_commands);

      Serial.println("🔗 Inscrito em tópicos de comando.");

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