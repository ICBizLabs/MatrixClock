#pragma once
#include <Arduino.h>

// Serial + in-memory ring buffer logging. The ring buffer is exposed through GET /api/log.
void   log_begin();
void   log_printf(char level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
String log_dump();        // contents of the ring buffer, oldest line first

#define LOGI(...) log_printf('I', __VA_ARGS__)
#define LOGW(...) log_printf('W', __VA_ARGS__)
#define LOGE(...) log_printf('E', __VA_ARGS__)
