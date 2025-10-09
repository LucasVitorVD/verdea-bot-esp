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
const int PINO_BOIA = D5; // ✅ NOVO: Sensor de boia

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

struct WateringTime
{
  int hour;
  int minute;
  bool executed; // para controlar se já foi executado hoje
};

WateringTime wateringTimes[2]; // Suporta até 2 horários
int wateringTimesCount = 0;    // Quantos horários estão configurados
String wateringFrequency = "once_a_day";

bool isIrrigating = false;
bool pumpStatus = false;

unsigned long irrigationStart = 0;
unsigned long lastMoistureCheck = 0;
unsigned long lastWateringMillis = 0;

int soilMoisture = 0;

// ✅ CORREÇÃO: Variáveis para controle do horário agendado
int lastTriggeredDay = -1;        // Dia da última irrigação agendada
bool dailyIrrigationDone = false; // Flag para controlar se já irrigou hoje

unsigned long lastIrrigationEndTime = 0;
const unsigned long MIN_INTERVAL_BETWEEN_IRRIGATIONS = 300000; // 5 minutos em ms

String lastDisplayedMode = "";
int lastDisplayedMoisture = -1;

bool waterLevelOk = false; // Flag para nível de água

// ✅ Controle do botão e display MAC
const int BUTTON_PIN = D7;
bool mostrarMAC = false; // false = irrigação, true = MAC
int prev_button_state = HIGH;
String macAddress = "";
String mensagemLetreiro = "";
int posLetreiro = 0;
unsigned long lastScroll = 0;
const unsigned long scrollInterval = 600; // ms entre cada movimento

// ✅ Controle de atualização do display
unsigned long lastDisplayRefresh = 0;
const unsigned long displayRefreshInterval = 500;

// ================= FUNÇÕES =================
void publishIrrigationHistory(double soilMoisture, String mode, int durationSeconds);

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

    for (int i = 0; i < wateringTimesCount; i++)
    {
      wateringTimes[i].executed = false;
    }

    lastIrrigationEndTime = 0;

    Serial.printf("🗓️ Novo dia detectado (Dia %d) - Irrigação liberada\n", currentDay);
  }

  lastTriggeredDay = currentDay;
}

bool checkWaterLevel()
{
  int leituraBoia = digitalRead(PINO_BOIA);
  waterLevelOk = (leituraBoia == LOW); // LOW = nível OK
  return waterLevelOk;
}

void controlPump(bool ligar, String motivo)
{
  // ✅ SEGURANÇA: Só liga bomba se houver água
  if (ligar && !waterLevelOk)
  {
    Serial.println("🚨 ERRO: Nível de água BAIXO! Bomba bloqueada por segurança.");
    digitalWrite(PINO_RELE, HIGH); // Força desligado
    pumpStatus = false;

    // Atualizar LCD com alerta
    lcd.setCursor(0, 0);
    lcd.print("NIVEL BAIXO!    ");
    return;
  }

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

void atualizarDisplayMAC()
{
  // Linha 0 fixa
  lcd.setCursor(0, 0);
  lcd.print("ENDERECO MAC:   ");

  // Linha 1: letreiro rolando
  if (millis() - lastScroll >= scrollInterval)
  {
    lastScroll = millis();
    lcd.setCursor(0, 1);

    String displayText = mensagemLetreiro.substring(posLetreiro);
    if (displayText.length() < 16)
    {
      displayText += mensagemLetreiro.substring(0, 16 - displayText.length());
    }

    lcd.print(displayText);

    posLetreiro++;
    if (posLetreiro >= mensagemLetreiro.length())
      posLetreiro = 0;
  }
}

void handleButton()
{
  int button_state = digitalRead(BUTTON_PIN);

  // Detecta transição de HIGH para LOW (botão pressionado)
  if (prev_button_state == HIGH && button_state == LOW)
  {
    delay(50);                          // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) // Confirma pressionamento
    {
      mostrarMAC = !mostrarMAC;

      // ✅ LIMPA O DISPLAY COMPLETAMENTE
      lcd.clear();
      posLetreiro = 0;

      Serial.println("\n========================================");
      Serial.println(mostrarMAC ? "🔄 MODO: MAC ADDRESS" : "🔄 MODO: IRRIGAÇÃO");
      Serial.println("========================================");

      // ✅ FORÇA ATUALIZAÇÃO COMPLETA ao voltar para modo irrigação
      if (!mostrarMAC)
      {
        lastDisplayedMode = ""; // Força reescrever linha 0

        // ✅ CORREÇÃO: Lê umidade atual e já define como "exibida"
        soilMoisture = readSoilMoisture();
        lastDisplayedMoisture = soilMoisture; // ← IMPORTANTE: define como já exibida

        // Atualiza linha 0 (modo/status)
        lcd.setCursor(0, 0);
        if (!waterLevelOk)
        {
          lcd.print("NIVEL BAIXO!    ");
          Serial.println("⚠️ Display: NIVEL BAIXO!");
        }
        else if (irrigationMode == "SCHEDULED" && wateringTimesCount > 0)
        {
          // Encontrar o próximo horário não executado
          int nextScheduleIndex = -1;
          for (int i = 0; i < wateringTimesCount; i++)
          {
            if (!wateringTimes[i].executed)
            {
              nextScheduleIndex = i;
              break;
            }
          }

          if (nextScheduleIndex >= 0)
          {
            char timeBuffer[20];
            sprintf(timeBuffer, "AGENDADO %02d:%02d",
                    wateringTimes[nextScheduleIndex].hour,
                    wateringTimes[nextScheduleIndex].minute);
            lcd.print(String(timeBuffer) + "   ");
            Serial.println("📅 Display: " + String(timeBuffer));
          }
          else
          {
            char timeBuffer[20];
            sprintf(timeBuffer, "AGENDADO %02d:%02d",
                    wateringTimes[0].hour,
                    wateringTimes[0].minute);
            lcd.print(String(timeBuffer) + "   ");
            Serial.println("📅 Display: " + String(timeBuffer));
          }
        }
        else
        {
          lcd.print(irrigationMode + " Mode      ");
          Serial.println("⚙️ Display: " + irrigationMode + " Mode");
        }

        // Atualiza linha 1 (umidade)
        lcd.setCursor(0, 1);
        char moistureBuffer[20];
        sprintf(moistureBuffer, "Umidade: %d%%  ", soilMoisture);
        lcd.print(moistureBuffer);
        Serial.println("💧 Display: " + String(moistureBuffer));

        Serial.println("✅ Display atualizado completamente");
        Serial.println("========================================\n");
      }
      else
      {
        Serial.println("📟 Exibindo MAC Address no display");
        Serial.println("========================================\n");
      }
    }
  }

  prev_button_state = button_state;
}

void initIrrigation()
{
  Wire.begin(D2, D3);
  lcd.init();
  lcd.backlight();

  pinMode(PINO_RELE, OUTPUT);
  digitalWrite(PINO_RELE, HIGH);

  pinMode(PINO_BOIA, INPUT_PULLUP);

  // ✅ Configurar botão
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // ✅ Obter MAC Address
  macAddress = WiFi.macAddress();
  mensagemLetreiro = macAddress + "   "; // Espaços extras para efeito visual

  if (idealSoilMoisture == 0)
  {
    idealSoilMoisture = 40;
    Serial.printf("⚙️ Umidade ideal inicializada para: %d%%\n", idealSoilMoisture);
  }

  Serial.println("🌱 Sistema de irrigação iniciado.");
  Serial.println("📟 MAC Address: " + macAddress);

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

  // ✅ Verificar nível antes de iniciar
  if (!checkWaterLevel())
  {
    Serial.println("🚨 Impossível iniciar irrigação - nível de água baixo!");
    lcd.setCursor(0, 0);
    lcd.print("NIVEL BAIXO!    ");
    return;
  }

  isIrrigating = true;
  irrigationStart = millis();
  lastMoistureCheck = millis();
  controlPump(true, "Irrigação iniciada");
}

void stopIrrigation(String motivo)
{
  unsigned long irrigationDuration = (millis() - irrigationStart) / 1000;

  isIrrigating = false;
  lastWateringMillis = millis();
  lastIrrigationEndTime = millis(); // ✅ Registrar quando terminou
  controlPump(false, motivo);

  bool isSuccessful = (motivo == "Meta atingida");
  // Publicar histórico de irrigação
  if (isSuccessful && irrigationDuration >= 5)
  {
    publishIrrigationHistory(soilMoisture, irrigationMode, irrigationDuration);
  }
}

void handleIrrigation()
{
  checkWaterLevel();
  soilMoisture = readSoilMoisture();

  // ✅ Se estiver mostrando MAC, não atualizar display de irrigação
  if (mostrarMAC)
  {
    atualizarDisplayMAC();

    // Logs reduzidos quando em modo MAC (a cada 10 segundos)
    static unsigned long lastLogTimeMAC = 0;
    if (millis() - lastLogTimeMAC >= 10000)
    {
      Serial.printf("📟 Modo MAC | Umidade: %d%% | Nivel: %s | Bomba: %s\n",
                    soilMoisture,
                    waterLevelOk ? "OK" : "BAIXO",
                    pumpStatus ? "ON" : "OFF");
      lastLogTimeMAC = millis();
    }
  }
  else
  {
    // --- Modo IRRIGAÇÃO: Atualização do Display com Controle de Frequência ---

    // ✅ SÓ ATUALIZA DISPLAY A CADA 500ms (evita flickering)
    if (millis() - lastDisplayRefresh >= displayRefreshInterval)
    {
      lastDisplayRefresh = millis();

      // Atualizar linha 0 apenas se mudou o modo ou nível
      if (irrigationMode != lastDisplayedMode || !waterLevelOk)
      {
        lcd.setCursor(0, 0);

        if (!waterLevelOk)
        {
          lcd.print("NIVEL BAIXO!    ");
        }
        else if (irrigationMode == "SCHEDULED" && wateringTimesCount > 0)
        {
          // Encontrar o próximo horário não executado
          int nextScheduleIndex = -1;
          for (int i = 0; i < wateringTimesCount; i++)
          {
            if (!wateringTimes[i].executed)
            {
              nextScheduleIndex = i;
              break;
            }
          }

          // Se encontrou horário pendente, mostrar ele
          if (nextScheduleIndex >= 0)
          {
            lcd.print("AGENDADO " + String(wateringTimes[nextScheduleIndex].hour) + ":" +
                      (wateringTimes[nextScheduleIndex].minute < 10 ? "0" : "") +
                      String(wateringTimes[nextScheduleIndex].minute) + "   ");
          }
          else
          {
            // Todos executados, mostrar o primeiro
            lcd.print("AGENDADO " + String(wateringTimes[0].hour) + ":" +
                      (wateringTimes[0].minute < 10 ? "0" : "") +
                      String(wateringTimes[0].minute) + "   ");
          }
        }
        else
        {
          lcd.print(irrigationMode + " Mode      ");
        }
        lastDisplayedMode = waterLevelOk ? irrigationMode : "NIVEL_BAIXO";
      }

      // Atualizar linha 1 apenas se umidade mudou
      if (soilMoisture != lastDisplayedMoisture)
      {
        lcd.setCursor(0, 1);
        lcd.print("Umidade: " + String(soilMoisture) + "%  ");
        lastDisplayedMoisture = soilMoisture;
      }
    }

    // Logs periódicos (a cada 5 segundos)
    static unsigned long lastLogTime = 0;
    if (millis() - lastLogTime >= 5000)
    {
      if (irrigationMode == "AUTO")
      {
        String statusSolo = soilMoisture < idealSoilMoisture ? "Solo SECO" : "Solo OK";
        Serial.printf("🌱 [AUTO] %s | Umidade: %d%% | Alvo: %d%% | Nivel: %s | Bomba: %s\n",
                      statusSolo.c_str(), soilMoisture, idealSoilMoisture,
                      waterLevelOk ? "OK" : "BAIXO",
                      pumpStatus ? "LIGADA" : "DESLIGADA");
      }
      lastLogTime = millis();
    }
  }

  // ✅ Verificações de segurança (independente do modo de display)
  if (!waterLevelOk && isIrrigating)
  {
    stopIrrigation("Nivel baixo detectado");
    Serial.println("🚨 Irrigação interrompida - nível de água baixo!");
    return;
  }

  if (isIrrigating)
    return;

  if (!waterLevelOk)
    return;

  // --- Lógica de Disparo (funciona independente do display) ---
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

    // Verificar todos os horários configurados
    for (int i = 0; i < wateringTimesCount; i++)
    {
      WateringTime &wt = wateringTimes[i];

      if (wt.executed)
        continue;

      bool isScheduledTime = (currentHour == wt.hour &&
                              currentMinute >= wt.minute &&
                              currentMinute <= (wt.minute + 2));

      if (isScheduledTime)
      {
        unsigned long timeSinceLastIrrigation = millis() - lastIrrigationEndTime;

        if (lastIrrigationEndTime > 0 && timeSinceLastIrrigation < MIN_INTERVAL_BETWEEN_IRRIGATIONS)
        {
          Serial.printf("⚠️ Horário %d (%02d:%02d) - Irrigação muito recente (%lu segundos atrás)\n",
                        i + 1, wt.hour, wt.minute, timeSinceLastIrrigation / 1000);
          Serial.println("   Pulando este horário por segurança");
          wt.executed = true;
          lastDisplayedMode = "";
          continue;
        }

        Serial.printf("🎯 Horário %d agendado atingido (%02d:%02d) - verificando necessidade\n",
                      i + 1, wt.hour, wt.minute);

        if (soilMoisture < idealSoilMoisture)
        {
          Serial.printf("💧 Solo precisa de água (%d%% < %d%%) - iniciando irrigação\n",
                        soilMoisture, idealSoilMoisture);
          startIrrigation();
          wt.executed = true;
          lastTriggeredDay = currentDay;
          lastDisplayedMode = "";
          break;
        }
        else
        {
          Serial.printf("💧 Solo já está adequado (%d%% >= %d%%) - irrigação não necessária\n",
                        soilMoisture, idealSoilMoisture);
          wt.executed = true;
          lastTriggeredDay = currentDay;
          lastDisplayedMode = "";
        }
      }
    }

    // LOG PERIÓDICO (a cada 30 segundos)
    static unsigned long lastScheduledLog = 0;
    if (millis() - lastScheduledLog >= 30000)
    {
      Serial.print("⏰ [SCHEDULED] Hora: ");
      Serial.printf("%02d:%02d | Agendados: ", currentHour, currentMinute);
      for (int i = 0; i < wateringTimesCount; i++)
      {
        Serial.printf("%02d:%02d%s", wateringTimes[i].hour, wateringTimes[i].minute,
                      wateringTimes[i].executed ? "(✓)" : "");
        if (i < wateringTimesCount - 1)
          Serial.print(", ");
      }
      Serial.println();
      lastScheduledLog = millis();
    }
  }
}

void handleIrrigationTimer()
{
  if (!isIrrigating)
    return;

  checkWaterLevel();

  if (!waterLevelOk)
  {
    stopIrrigation("Nivel baixo");
    lastDisplayedMode = "";
    return;
  }

  unsigned long nowMs = millis();
  unsigned long elapsed = nowMs - irrigationStart;

  if (nowMs - lastMoistureCheck >= CHECK_INTERVAL)
  {
    soilMoisture = readSoilMoisture();
    lastMoistureCheck = nowMs;

    Serial.printf("⏱️ Irrigando... Umidade: %d%% | Tempo: %lus | Nivel: OK\n",
                  soilMoisture, elapsed / 1000);

    // ✅ Atualizar apenas linha 2 (umidade)
    lcd.setCursor(0, 1);
    lcd.print("Umidade: " + String(soilMoisture) + "%   ");
    lastDisplayedMoisture = soilMoisture;

    // Verificar se atingiu meta
    if (soilMoisture >= idealSoilMoisture + BUFFER_SEGURANCA)
    {
      stopIrrigation("Meta atingida");
      lastDisplayedMode = ""; // Forçar atualização para próximo horário
      return;
    }
  }

  // Verificar tempo máximo
  if (elapsed >= IRRIGATION_MAX_DURATION)
  {
    stopIrrigation("Tempo maximo");
    lastDisplayedMode = ""; // Forçar atualização para próximo horário
  }
}

void setIrrigationConfig(String mode, JsonArray times, String freq, int targetMoisture)
{
  if (WiFi.status() == WL_CONNECTED && !isTimeSynchronized())
  {
    setupTime();
  }

  irrigationMode = mode;
  wateringFrequency = freq;
  idealSoilMoisture = targetMoisture;

  // ✅ Processar array de horários
  wateringTimesCount = 0;
  for (JsonVariant timeValue : times)
  {
    if (wateringTimesCount >= 2)
      break; // Máximo 2 horários

    String timeStr = timeValue.as<String>();
    int sepIndex = timeStr.indexOf(':');
    if (sepIndex > 0)
    {
      wateringTimes[wateringTimesCount].hour = timeStr.substring(0, sepIndex).toInt();
      wateringTimes[wateringTimesCount].minute = timeStr.substring(sepIndex + 1).toInt();
      wateringTimes[wateringTimesCount].executed = false;
      wateringTimesCount++;
    }
  }

  // ✅ Parar irrigação ativa ao trocar para SCHEDULED
  if (mode == "SCHEDULED" && isIrrigating)
  {
    stopIrrigation("Modo alterado para SCHEDULED");
    Serial.println("🛑 Irrigação interrompida - aguardando horário agendado");
  }

  // Resetar flags
  dailyIrrigationDone = false;
  lastTriggeredDay = -1;
  lastIrrigationEndTime = 0;

  lastDisplayedMode = "";

  Serial.printf("⚙️ Nova config: %s | %d horário(s) | Alvo: %d%%\n",
                mode.c_str(), wateringTimesCount, targetMoisture);

  for (int i = 0; i < wateringTimesCount; i++)
  {
    Serial.printf("   Horário %d: %02d:%02d\n", i + 1,
                  wateringTimes[i].hour, wateringTimes[i].minute);
  }
}

// ✅ Função reset
void resetIrrigationConfig()
{
  irrigationMode = "AUTO";
  idealSoilMoisture = 40; // padrão razoável
  wateringTimesCount = 0;
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