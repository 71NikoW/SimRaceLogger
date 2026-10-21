#pragma once
// Minimal stand-ins so src/main.cpp can be compile-checked on the PC.
// Not a simulation: nothing here is executed.
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1

struct HardwareSerialStub {
  void begin(unsigned long) {}
  int available() { return 0; }
  int read() { return -1; }
  int printf(const char*, ...) { return 0; }
  size_t println() { return 0; }
};
extern HardwareSerialStub Serial;
extern HardwareSerialStub Serial0;

inline uint32_t millis() { return 0; }
inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void digitalWrite(int, int) {}
inline bool ledcAttach(int, uint32_t, uint8_t) { return true; }
inline bool ledcWrite(int, uint32_t) { return true; }
