#ifndef IRRIGATION_CONTROLLER_H
#define IRRIGATION_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TimeLib.h>
#include "time_utils.h"

// ================= CONFIG =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pinos
const int PINO_SENSOR = A0;
const int PINO_RELE = D1;

// Calibração sensor
const int VALOR_SECO = 1024;
const int VALOR_MOLHADO = 660;

// Segurança
const unsigned long IRRIGATION_MAX_DURATION = 60000; // 60s
const unsigned long CHECK_INTERVAL = 5000;           // Checa umidade a cada 5s
const int BUFFER_SEGURANCA = 10;                     // +10% além do ideal

// ================= VARIÁVEIS =================
String irrigationMode = "AUTO"; // AUTO ou SCHEDULED
int idealSoilMoisture = 0;      // alvo %

int wateringHour = 0;
int wateringMinute = 0;
String wateringFrequency = "once_a_day";

bool isIrrigating = false;
bool pumpStatus = false;

unsigned long irrigationStart = 0;
unsigned long lastMoistureCheck = 0;
unsigned long lastWateringMillis = 0;

int soilMoisture = 0;

String lastStatusSolo = "";
int lastSoilMoisture = -1;

// ✅ CORREÇÃO: Variáveis para controle do horário agendado
int lastTriggeredDay = -1;        // Dia da última irrigação agendada
bool dailyIrrigationDone = false; // Flag para controlar se já irrigou hoje

// ================= FUNÇÕES =================
struct tm getCurrentTime()
{
  time_t nowTime = time(nullptr);
  struct tm timeinfo;
  localtime_r(&nowTime, &timeinfo);
  return timeinfo;
}

int getCurrentDay()
{
  struct tm timeinfo = getCurrentTime();
  return timeinfo.tm_mday;
}

void checkDayChange()
{
  if (!isTimeSynchronized())
    return;

  int currentDay = getCurrentDay();

  if (lastTriggeredDay != -1 && currentDay != lastTriggeredDay)
  {
    dailyIrrigationDone = false;
    Serial.printf("🗓️ Novo dia detectado (Dia %d) - Irrigação liberada\n", currentDay);
  }

  lastTriggeredDay = currentDay;
}

void controlPump(bool ligar, String motivo)
{
  digitalWrite(PINO_RELE, ligar ? LOW : HIGH);
  pumpStatus = ligar;
  Serial.printf("💧 Bomba %s | %s\n", ligar ? "LIGADA" : "DESLIGADA", motivo.c_str());
}

int readSoilMoisture()
{
  int leitura = analogRead(PINO_SENSOR);
  int umidade = map(leitura, VALOR_MOLHADO, VALOR_SECO, 100, 0);
  return constrain(umidade, 0, 100);
}

unsigned long getFrequencyMillis(String freq)
{
  if (freq == "once_a_day")
    return 24UL * 3600000UL;
  if (freq == "twice_a_day")
    return 12UL * 3600000UL;
  if (freq == "every_2_days")
    return 48UL * 3600000UL;
  if (freq == "weekly")
    return 7UL * 24UL * 3600000UL;
  return 24UL * 3600000UL; // padrão: 1x/dia
}

void initIrrigation()
{
  Wire.begin(D2, D3);
  lcd.init();
  lcd.backlight();

  pinMode(PINO_RELE, OUTPUT);
  digitalWrite(PINO_RELE, HIGH); // desligado

  if (idealSoilMoisture == 0)
  {
    idealSoilMoisture = 40; // Valor padrão seguro
    Serial.printf("⚙️ Umidade ideal inicializada para: %d%%\n", idealSoilMoisture);
  }

  Serial.println("🌱 Sistema de irrigação iniciado.");
  lcd.setCursor(0, 0);
  lcd.print("Sistema pronto");
}

void startIrrigation()
{
  if (isIrrigating)
  {
    Serial.println("⚠️ Tentativa de iniciar irrigação já em andamento");
    return;
  }

  isIrrigating = true;
  irrigationStart = millis();
  lastMoistureCheck = millis();
  controlPump(true, "Irrigação iniciada");
}

void stopIrrigation(String motivo)
{
  isIrrigating = false;
  lastWateringMillis = millis();
  controlPump(false, motivo);
}

void handleIrrigation()
{
  soilMoisture = readSoilMoisture();

  // --- Atualização do Display ---
  static String lastDisplayedMode = "";
  static int lastDisplayedMoisture = -1;
  static unsigned long lastLogTime = 0;

  if (irrigationMode != lastDisplayedMode)
  {
    lcd.setCursor(0, 0);
    if (irrigationMode == "SCHEDULED")
    {
      lcd.print("AGENDADO " + String(wateringHour) + ":" +
                (wateringMinute < 10 ? "0" : "") + String(wateringMinute) + "   ");
    }
    else
    {
      lcd.print(irrigationMode + " Mode      ");
    }
    lastDisplayedMode = irrigationMode;
  }

  if (soilMoisture != lastDisplayedMoisture)
  {
    lcd.setCursor(0, 1);
    lcd.print("Umidade: " + String(soilMoisture) + "%  ");
    lastDisplayedMoisture = soilMoisture;
  }

  // Logs periódicos (a cada 5 segundos)
  if (millis() - lastLogTime >= 5000)
  {
    if (irrigationMode == "AUTO")
    {
      String statusSolo = soilMoisture < idealSoilMoisture ? "Solo SECO" : "Solo OK";
      Serial.printf("🌱 [AUTO] %s | Umidade: %d%% | Alvo: %d%% | Bomba: %s\n",
                    statusSolo.c_str(), soilMoisture, idealSoilMoisture,
                    pumpStatus ? "LIGADA" : "DESLIGADA");
    }
    lastLogTime = millis();
  }

  if (isIrrigating)
    return;

  // --- Lógica de Disparo ---
  if (irrigationMode == "AUTO")
  {
    if (soilMoisture < idealSoilMoisture)
    {
      Serial.printf("🚨 SOLO SECO! Umidade: %d%% < Alvo: %d%% - INICIANDO IRRIGAÇÃO\n",
                    soilMoisture, idealSoilMoisture);
      startIrrigation();
    }
  }
  else if (irrigationMode == "SCHEDULED")
  {
    if (!isTimeSynchronized())
    {
      Serial.println("⚠️ Horário não sincronizado, aguardando...");
      return;
    }

    checkDayChange();

    struct tm timeinfo = getCurrentTime();
    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentDay = timeinfo.tm_mday;

    bool isScheduledTime = (currentHour == wateringHour &&
                            currentMinute >= wateringMinute &&
                            currentMinute <= (wateringMinute + 2));
    bool isPastScheduledTime = (currentHour > wateringHour ||
                                (currentHour == wateringHour && currentMinute > wateringMinute));

    // ✅ LOGS MENOS FREQUENTES - só a cada 30 segundos no modo SCHEDULED
    static unsigned long lastScheduledLog = 0;
    if (millis() - lastScheduledLog >= 30000) // 30 segundos
    {
      Serial.printf("⏰ [SCHEDULED] Hora: %02d:%02d | Agendado: %02d:%02d | Irrigou hoje: %s\n",
                    currentHour, currentMinute, wateringHour, wateringMinute,
                    dailyIrrigationDone ? "SIM" : "NÃO");
      lastScheduledLog = millis();
    }

    // ✅ SÓ IRRIGAR SE FOR O HORÁRIO EXATO E NÃO IRRIGOU AINDA HOJE
    if (!dailyIrrigationDone && isScheduledTime)
    {
      Serial.println("🎯 Horário agendado atingido - verificando necessidade de irrigação");

      // Verificar se realmente precisa irrigar
      if (soilMoisture < idealSoilMoisture)
      {
        Serial.printf("💧 Solo precisa de água (%d%% < %d%%) - iniciando irrigação\n",
                      soilMoisture, idealSoilMoisture);
        startIrrigation();
        dailyIrrigationDone = true;
        lastTriggeredDay = currentDay;
      }
      else
      {
        Serial.printf("💧 Solo já está adequado (%d%% >= %d%%) - irrigação não necessária\n",
                      soilMoisture, idealSoilMoisture);
        dailyIrrigationDone = true;
        lastTriggeredDay = currentDay;
      }
    }
  }
}

void handleIrrigationTimer()
{
  if (!isIrrigating)
    return;

  unsigned long nowMs = millis();
  unsigned long elapsed = nowMs - irrigationStart;

  if (nowMs - lastMoistureCheck >= CHECK_INTERVAL)
  {
    soilMoisture = readSoilMoisture();
    lastMoistureCheck = nowMs;

    // Atualizar status do solo
    String statusSolo = soilMoisture < max(30, idealSoilMoisture) ? "Solo SECO" : "Solo OK";

    Serial.printf("⏱️ Irrigando... %s | Umidade: %d%% | Tempo: %lus\n", statusSolo.c_str(), soilMoisture, elapsed / 1000);

    // Atualizar LCD só se mudou
    if (statusSolo != lastStatusSolo || soilMoisture != lastSoilMoisture)
    {
      lcd.setCursor(0, 0);
      lcd.print(statusSolo + "       "); // limpar sobra
      lcd.setCursor(0, 1);
      lcd.print("Umidade: " + String(soilMoisture) + "%   ");

      lastStatusSolo = statusSolo;
      lastSoilMoisture = soilMoisture;
    }

    if (soilMoisture >= idealSoilMoisture + BUFFER_SEGURANCA)
    {
      stopIrrigation("Meta atingida");
      return;
    }
  }

  if (elapsed >= IRRIGATION_MAX_DURATION)
  {
    stopIrrigation("Tempo maximo");
  }
}

void setIrrigationConfig(String mode, int hour, int minute, String freq, int targetMoisture)
{
  if (WiFi.status() == WL_CONNECTED && !isTimeSynchronized())
  {
    setupTime();
  }

  irrigationMode = mode;
  wateringHour = hour;
  wateringMinute = minute;
  wateringFrequency = freq;
  idealSoilMoisture = targetMoisture;

  // ✅ ADICIONAR: Parar irrigação ativa ao trocar para SCHEDULED
  if (mode == "SCHEDULED" && isIrrigating)
  {
    stopIrrigation("Modo alterado para SCHEDULED");
    Serial.println("🛑 Irrigação interrompida - aguardando horário agendado");
  }

  // Resetar flags
  dailyIrrigationDone = false;
  lastTriggeredDay = -1;

  Serial.printf("⚙️ Nova config: %s %02d:%02d %d%%\n",
                mode.c_str(), hour, minute, targetMoisture);
}

// ✅ Função reset
void resetIrrigationConfig()
{
  irrigationMode = "AUTO";
  idealSoilMoisture = 40; // padrão razoável
  wateringHour = 0;
  wateringMinute = 0;
  wateringFrequency = "once_a_day";
  isIrrigating = false;
  pumpStatus = false;

  // ✅ CORREÇÃO: Reset das flags de agendamento
  dailyIrrigationDone = false;
  lastTriggeredDay = -1;

  stopIrrigation("Config resetada");
  Serial.println("🔄 Configuração resetada para padrão AUTO");
}

#endif