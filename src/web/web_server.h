#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

namespace web {
  void begin();
  void registerApi(AsyncWebServer& server);   // implemented in api_handlers.cpp
  void sendJsonError(AsyncWebServerRequest* r, int code, const String& msg);
}
