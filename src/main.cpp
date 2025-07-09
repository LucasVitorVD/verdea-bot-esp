#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);
WiFiManager wifiManager;

const char *apiUrl = "http://192.168.15.115:8080/api/device/add";
String lastSentMac = "";
bool alreadyRegistered = false;

void sendDeviceRegistration()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    WiFiClient client;
    http.begin(client, String(apiUrl));
    http.addHeader("Content-Type", "application/json");

    String mac = WiFi.macAddress();
    Serial.print("MAC do dispositivo: ");
    Serial.println(mac);

    // Gera um nome baseado no MAC (sem os dois primeiros blocos)
    String macSuffix = mac;
    macSuffix.replace(":", "");
    macSuffix = macSuffix.substring(6);
    String deviceName = "dispositivo-verdea-" + macSuffix;

    String deviceStatus = "ONLINE";

    // Monta o JSON manualmente
    String payload = "{\"name\": \"" + deviceName + "\", \"macAddress\": \"" + mac + "\", \"status\": \"" + deviceStatus + "\"}";

    int httpCode = http.POST(payload);

    if (httpCode > 0)
    {
      Serial.println("📡 Requisição enviada!");
      Serial.print("Código HTTP: ");
      Serial.println(httpCode);

      String response = http.getString();
      Serial.println("Resposta da API:");
      Serial.println(response);

      alreadyRegistered = true;
      lastSentMac = mac;
    }
    else
    {
      Serial.print("❌ Erro na requisição: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }

    http.end();
  }
  else
  {
    Serial.println("❌ Não conectado ao Wi-Fi. Verifique a conexão.");
  }
}

void setup()
{
  Serial.begin(921600);
  delay(1000);
  Serial.println("Iniciando dispositivo Verdea\n");

  WiFiManager wifiManager;

  if (!wifiManager.autoConnect("VERDEA-SETUP", "verdea123"))
  {
    Serial.println("Falha na conexão. Reiniciando...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("✅ Conectado ao Wi-Fi");
  Serial.print("IP Local: ");
  Serial.println(WiFi.localIP());

  sendDeviceRegistration();

  // Endpoint para resetar Wi-Fi remotamente
  server.on("/reset-wifi", HTTP_POST, []() {
    server.send(200, "text/plain", "Wi-Fi resetado. Reiniciando...");
    delay(1000);
    ::wifiManager.resetSettings();
    ESP.restart();
  });
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado. Reconectando...");
    WiFi.reconnect();
    delay(5000); // Aguarda 5 segundos para reconectar
  } else {
    if (!alreadyRegistered || WiFi.macAddress() != lastSentMac) {
      sendDeviceRegistration();
    }
  }

  delay(10000); // Aguarda 10 segundos antes de verificar novamente
}