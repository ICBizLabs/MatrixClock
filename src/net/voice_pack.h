#pragma once
#include <Arduino.h>
#include "config/config.h"

// Downloads the voice pack advertised by the update manifest ("voice" object) into LittleFS (/voice.pack), verifying
// the MD5, and hands it to audio/voice.cpp. Runs inside the network task after the updater.
namespace voice_pack {
  enum class State : uint8_t { Idle, NoPack, Installed, Downloading, Verifying, Error };
  struct Status {
    State state;
    uint8_t progress;             // download percent
    uint32_t available_version;   // from the manifest, 0 = none advertised
    uint32_t last_attempt_ms;     // 0 = never
    char err[64];
  };
  void begin();
  void requestDownload();         // web UI / API: fetch the manifest and (re)download even when the version matches
  void run(const AppConfig& cfg);
  Status status();
  const char* stateName(State s);
}
