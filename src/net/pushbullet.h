#pragma once
#include <Arduino.h>
#include "config/config.h"

// Pushbullet: outgoing notifications (queued here, sent by the network task) and incoming pushes shown on the panel.
namespace pushbullet {
  struct Status { bool configured; bool device_ok; uint32_t sent, received, errors; char last_err[48]; uint32_t last_poll_ms; };
  void begin();
  bool notify(const char* title, const char* body);   // queue a push to the account (any task); false when not configured
  void runQueued(const AppConfig& cfg);               // network task: send queued pushes
  void poll(const AppConfig& cfg);                    // network task: fetch new incoming pushes
  bool due(const AppConfig& cfg, uint32_t now_ms);    // is a poll due
  Status status();
}
