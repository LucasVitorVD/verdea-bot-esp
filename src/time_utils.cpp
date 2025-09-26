#include "time_utils.h"

void setupTime()
{
  Serial.print("⏰ Configurando horário de Brasília (GMT-3)...");

  // ✅ Configurar timezone para Brasília
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov", "a.st1.ntp.br");

  // Aguardar sincronização com timeout maior
  time_t nowTime = time(nullptr);
  int attempts = 0;
  while (nowTime < 1000000000 && attempts < 30) // ✅ Valor mais realista
  {
    delay(500);
    Serial.print(".");
    nowTime = time(nullptr);
    attempts++;
  }

  if (nowTime < 1000000000)
  {
    Serial.println(" ❌ Falha na sincronização!");
    return;
  }

  // Verificar horário sincronizado
  struct tm timeinfo;
  localtime_r(&nowTime, &timeinfo);
  Serial.printf(" ✅\n");
  Serial.printf("✅ Horário sincronizado: %02d/%02d/%d %02d:%02d:%02d (GMT-3)\n",
                timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

bool isTimeSynchronized()
{
  time_t nowTime = time(nullptr);
  return (nowTime > 1000000000); // ✅ Valor mais realista
}