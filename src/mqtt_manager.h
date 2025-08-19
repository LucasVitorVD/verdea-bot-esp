#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

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

// Tópicos
const char *topic_status = "verdea/status";
const char *topic_commands = "verdea/commands";

// Instâncias globais para o cliente MQTT
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// Declarações das funções
void initMQTT();
void handleMQTT();
void reconnectMQTT();
void publishStatus(String payload);
void mqttCallback(char *topic, byte *payload, unsigned int length);

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

  // Lógica para comandos de irrigação
  if (String(topic) == topic_commands)
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

  // Lógica para comandos específicos do dispositivo
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String deviceTopicCommands = "verdea/commands/" + mac;

  if (String(topic) == deviceTopicCommands)
  {
    if (msg == "ON")
    {
      controlPump(true);
      Serial.println("💧 Comando de LIGAR bomba recebido (tópico específico).");
    }
    else if (msg == "OFF")
    {
      controlPump(false);
      Serial.println("💧 Comando de DESLIGAR bomba recebido (tópico específico).");
    }
    else if (msg == "RESET_WIFI")
    {
      Serial.println("🚨 Comando de RESET_WIFI recebido via MQTT (tópico específico)!");
      delay(1000);
      resetWiFiSettings();
      ESP.restart();
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

    String clientId = "NodeMCU-" + WiFi.macAddress();
    clientId.replace(":", "");

    if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("✅ Conectado!");

      // Tópicos de comando dinâmicos e estáticos
      String mac = WiFi.macAddress();
      mac.replace(":", "");
      String deviceTopicCommands = "verdea/commands/" + mac;

      // Inscrição em tópicos
      mqttClient.subscribe(deviceTopicCommands.c_str());
      mqttClient.subscribe(topic_commands);

      Serial.println("🔗 Inscrito em tópicos de comando.");

      // Publicar status de conexão inicial
      String deviceName = "irrigacao-verdea-" + mac.substring(6);
      String statusMsg = "{\"device\":\"" + deviceName + "\",\"status\":\"online\",\"timestamp\":" + String(millis()) + "}";
      publishStatus(statusMsg);
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

void publishStatus(String payload)
{
  if (mqttClient.connected())
  {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String deviceTopicStatus = "verdea/status/" + mac;

    // Publica status no tópico específico do dispositivo
    bool result = mqttClient.publish(deviceTopicStatus.c_str(), payload.c_str());

    if (result)
    {
      Serial.println("📤 Status publicado com sucesso");
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