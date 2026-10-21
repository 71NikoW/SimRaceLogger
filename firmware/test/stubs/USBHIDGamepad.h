#pragma once
#include <stdint.h>
class USBHIDGamepad {
 public:
  void begin() {}
  bool send(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, uint8_t, uint32_t) { return true; }
};
