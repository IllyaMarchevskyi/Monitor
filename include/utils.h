#pragma once
#include "serial.h"
#include <Arduino.h>
#include <string.h>

#define ENABLE_TIMING 1

#if ENABLE_TIMING
#define TIME_CALL(label, call_expr)                                            \
  do {                                                                         \
    uint32_t __t0 = micros();                                                  \
    uint32_t __t0m = millis();                                                 \
    (void)(call_expr);                                                         \
    uint32_t __dus = micros() - __t0;                                          \
    uint32_t __dms = millis() - __t0m;                                         \
    if (__dms > 100) {                                                         \
      logLine(F("[T] "), false);                                               \
      logLine(label, false);                                                   \
      logLine(F(": "), false);                                                 \
      logLine(__dus, false);                                                   \
      logLine(F(" us  (~"), false);                                            \
      logLine(__dms, false);                                                   \
      logLine(F(" ms)"), true);                                                \
    }                                                                          \
  } while (0)
#else
#define TIME_CALL(label, call_expr) (void)(call_expr)
#endif

constexpr uint8_t SAMPLES_PER_MIN = 60;

bool time_guard_allow(const char *key, uint32_t interval_ms,
                      bool wait_first = false);

struct _TimeGuardEntry {
  char key[32];
  uint32_t last_ms;
};

bool rs485_acquire(uint16_t timeout_ms = 100);
void rs485_release();

size_t buildMbTcpRead03(uint8_t *out, uint16_t txId, uint8_t unit,
                        uint16_t addr, uint16_t qty);
void printHex(const uint8_t *b, size_t n);
void pre_transmission_main();
void post_transmission_main();
void collectAndAverageEveryMinute();

template <typename T> void fill(T *mass, size_t count, T value) {
  for (size_t i = 0; i < count; ++i)
    mass[i] = value;
}
