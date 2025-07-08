#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>

const char* apiUrl = "http://192.168.15.115:8080/api/device/add";

void setup() {
  Serial.begin(921600);
  delay(1000);
  Serial.println("Iniciando dispositivo Verdea\n");

  WiFiManager wifiManager;

  if (!wifiManager.autoConnect("VERDEA-SETUP", "verdea123")) {
    Serial.println("Falha na conexão. Reiniciando...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("✅ Conectado ao Wi-Fi");
  Serial.print("IP Local: ");
  Serial.println(WiFi.localIP());

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;
    http.begin(client, String(apiUrl));
    http.addHeader("Content-Type", "application/json");

    // Obtem o MAC address do ESP
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

    if (httpCode > 0) {
      Serial.println("📡 Requisição enviada!");
      Serial.print("Código HTTP: ");
      Serial.println(httpCode);

      String response = http.getString();
      Serial.println("Resposta da API:");
      Serial.println(response);
    } else {
      Serial.print("❌ Erro na requisição: ");
      Serial.println(http.errorToString(httpCode).c_str());
      Serial.println(httpCode);
    }

    http.end();
  } else {
    Serial.println("⚠️ Não conectado ao Wi-Fi");
  }
}

void loop() {
  // Por enquanto nada aqui
}