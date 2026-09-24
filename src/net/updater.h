#pragma once
#include <Arduino.h>
#include "config/config.h"

// Self-update: reads the web installer's manifest.json, compares versions, downloads the OTA image over HTTPS,
// verifies the MD5 from the manifest and installs it. Runs inside the network task.
namespace updater {
  enum class State : uint8_t { Idle, Checking, UpToDate, Available, Downloading, Installing, Done, Error };
  struct Status {
    State state;
    char latest[16];
    char file[80];
    uint32_t size;
    uint8_t progress;           // download percent
    uint32_t last_check_ms;     // 0 = never
    char err[64];
  };
  struct VoiceInfo {            // optional "voice" object of the manifest (voice pack built by tools/make_voice_pack.py)
    bool present = false;
    char file[64] = "";
    char md5[40] = "";
    char voice[32] = "";
    uint32_t size = 0;
    uint32_t version = 0;
    uint16_t format = 0;
  };
  void begin();
  void requestCheck();          // web UI / API: check now
  void requestInstall();        // web UI / API: install the available version now
  void run(const AppConfig& cfg);
  Status status();
  VoiceInfo voiceInfo();        // from the last successfully parsed manifest
  String manifestBase();        // manifest URL up to and including the last '/' (files are relative to it)
  bool manifestSeen();          // a manifest has been parsed since boot
  const char* stateName(State s);
  bool versionNewer(const char* remote, const char* local);   // semantic "a.b.c" compare
}
