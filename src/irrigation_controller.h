#ifndef IRRIGATION_CONTROLLER_H
#define IRRIGATION_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"

// Variáveis globais para irrigação
extern bool pumpStatus;
extern int soilMoisture;
extern unsigned long lastIrrigationCheck;
extern unsigned long irrigationStartTime;
extern bool irrigationActive;

// Declarações das funções
void initIrrigation();
void handleIrrigation();
void offlineIrrigationLogic();
int readSoilMoisture();
void controlPump(bool turnOn);
bool getPumpStatus();
int getSoilMoisture();
bool isIrrigationActive();

// Implementações
void initIrrigation() {
  pumpStatus = false;
  soilMoisture = 0;
  lastIrrigationCheck = 0;
  irrigationStartTime = 0;
  irrigationActive = false;
  
  // Aqui você inicializaria os pinos dos sensores e relés
  // pinMode(PUMP_PIN, OUTPUT);
  // pinMode(SOIL_SENSOR_PIN, INPUT);
  
  Serial.println("🌱 Sistema de irrigação inicializado");
}

void handleIrrigation() {
  // Lógica de irrigação offline
  offlineIrrigationLogic();
  
  // Aqui você poderia adicionar lógica online também
  // if (getOnlineMode()) {
  //   handleOnlineIrrigation();
  // }
}

void offlineIrrigationLogic() {
  if (!getOfflineMode()) return;

  // Verificar irrigação a cada intervalo definido
  if (millis() - lastIrrigationCheck >= IRRIGATION_CHECK_INTERVAL) {
    lastIrrigationCheck = millis();

    soilMoisture = readSoilMoisture();
    Serial.println("🌱 Umidade do solo: " + String(soilMoisture) + "%");

    // Lógica de irrigação offline
    if (soilMoisture < DEFAULT_MOISTURE_THRESHOLD && !irrigationActive) {
      Serial.println("🚨 Solo seco detectado - iniciando irrigação offline");
      controlPump(true);
    } else if (irrigationActive && (millis() - irrigationStartTime >= DEFAULT_IRRIGATION_DURATION)) {
      Serial.println("⏰ Tempo de irrigação concluído - desligando bomba");
      controlPump(false);
    } else if (soilMoisture >= DEFAULT_MOISTURE_THRESHOLD + 10 && irrigationActive) {
      Serial.println("✅ Solo adequadamente hidratado - desligando bomba");
      controlPump(false);
    }
  }
}

int readSoilMoisture() {
  // Aqui você colocaria a leitura real do sensor
  // return analogRead(SOIL_SENSOR_PIN);
  
  // Simulação para teste
  return random(0, 100);
}

void controlPump(bool turnOn) {
  pumpStatus = turnOn;

  // Aqui você controlaria o relé real
  // digitalWrite(PUMP_PIN, turnOn ? HIGH : LOW);

  Serial.println("💧 Bomba " + String(turnOn ? "LIGADA" : "DESLIGADA"));

  if (turnOn) {
    irrigationStartTime = millis();
    irrigationActive = true;
  } else {
    irrigationActive = false;
  }
}

bool getPumpStatus() {
  return pumpStatus;
}

int getSoilMoisture() {
  return soilMoisture;
}

bool isIrrigationActive() {
  return irrigationActive;
}

// Variáveis globais
bool pumpStatus = false;
int soilMoisture = 0;
unsigned long lastIrrigationCheck = 0;
unsigned long irrigationStartTime = 0;
bool irrigationActive = false;

#endif