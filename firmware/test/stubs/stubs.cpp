#include "Arduino.h"
HardwareSerialStub Serial;
HardwareSerialStub Serial0;
// main.cpp defines setup()/loop(); call them once so the linker keeps them.
void setup();
void loop();
int main() {
  setup();
  loop();
  return 0;
}
