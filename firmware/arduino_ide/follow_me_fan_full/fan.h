#pragma once
#include <Arduino.h>

// SimonK ESC driver: mode 0 = off (1000 us), 1..3 = speed modes, ramped.
namespace fan {
bool begin();  // blocks for the ESC arming time
void setMode(uint8_t mode);
uint8_t mode();
void stopNow();
void update();
uint16_t outputUs();
}  // namespace fan
