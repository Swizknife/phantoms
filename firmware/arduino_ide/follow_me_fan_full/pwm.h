#pragma once
#include <Arduino.h>
#include <Servo.h>

#include "config.h"

// The Uno has no LEDC; the Servo library (Timer1) gives the 50 Hz RC pulses on D9/D10.
// The first pulse is set BEFORE attach() so the ESC never sees the library's 1500 us default.
inline void pwmBegin(Servo &s, uint8_t pin, uint16_t firstUs) {
  s.writeMicroseconds(firstUs);
  s.attach(pin, cfg::PULSE_ATTACH_MIN_US, cfg::PULSE_ATTACH_MAX_US);
}

inline void pwmWriteUs(Servo &s, uint16_t us) { s.writeMicroseconds(us); }
