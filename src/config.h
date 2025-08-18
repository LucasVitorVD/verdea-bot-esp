#ifndef CONFIG_H
#define CONFIG_H

// Configurações da API
const char *API_URL = "http://192.168.15.115:8080/api/device";
const char *HEARTBEAT_URL = "http://192.168.15.115:8080/api/device/heartbeat";

// Configurações de conectividade
const unsigned long CONNECTION_TIMEOUT = 30000; // 30 segundos
const unsigned long HEARTBEAT_INTERVAL = 60000; // 1 minuto
const unsigned long HEARTBEAT_TIMEOUT = 300000; // 5 minutos
const int MAX_RECONNECT_ATTEMPTS = 5;

// Configurações de irrigação offline
const int DEFAULT_MOISTURE_THRESHOLD = 30;
const int DEFAULT_IRRIGATION_DURATION = 30000; // 30 segundos
const unsigned long IRRIGATION_CHECK_INTERVAL = 60000; // 1 minuto

// Configurações WiFi
const char *WIFI_AP_NAME = "VERDEA-SETUP";
const char *WIFI_AP_PASSWORD = "verdeasetup";
const char *RESET_PASSWORD = "verdea2024";

// Configurações de log
const unsigned long STATUS_LOG_INTERVAL = 300000; // 5 minutos

// Configurações de timeout
const int HTTP_TIMEOUT = 10000; // 10 segundos
const int HEARTBEAT_HTTP_TIMEOUT = 5000; // 5 segundos
const int WIFI_CONFIG_TIMEOUT = 300; // 5 minutos
const int WIFI_CONNECT_TIMEOUT = 30; // 30 segundos

// Configurações de autenticação básica para o servidor web
const char* ESP_BASIC_AUTH_USER = "admin";
const char* ESP_BASIC_AUTH_PASS = "verdea123";

#endif