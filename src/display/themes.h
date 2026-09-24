#pragma once
#include <Arduino.h>
#include <time.h>

// Holiday colour themes with a matching decoration effect, selected by the local date.
namespace themes {
  enum class Deco : uint8_t { None, Snow, Confetti, Hearts, Sparkle };
  struct Theme {
    const char* name;
    uint32_t time, date, text;   // RGB888 colour overrides
    Deco deco;
  };
  const Theme* forDate(const struct tm& lt);   // nullptr when today is not a holiday
}
