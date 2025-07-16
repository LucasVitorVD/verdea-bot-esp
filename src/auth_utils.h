#ifndef AUTH_UTILS_H
#define AUTH_UTILS_H

#include <ESP8266WebServer.h>
#include "base64_utils.h"
#include "config.h"

bool checkBasicAuth(ESP8266WebServer &server) {
  if (!server.hasHeader("Authorization")) {
    server.sendHeader("WWW-Authenticate", "Basic realm=\"ESP8266\"");
    server.send(401, "text/plain", "Authorization required");
    return false;
  }

  String authHeader = server.header("Authorization");
  if (!authHeader.startsWith("Basic ")) {
    server.send(400, "text/plain", "Bad Authorization header");
    return false;
  }

  String encoded = authHeader.substring(6);
  char decoded[128]; // ajuste conforme necessário
  base64_decode(decoded, (char *)encoded.c_str(), encoded.length());

  String credentials = String(ESP_BASIC_AUTH_USER) + ":" + String(ESP_BASIC_AUTH_PASS);
  if (String(decoded) != credentials) {
    server.send(403, "text/plain", "Forbidden");
    return false;
  }

  return true;
}

#endif