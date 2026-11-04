#include "fan.h"

#include <math.h>

#include "config.h"
#include "pwm.h"

namespace {

Servo gEsc;
float gTargetUs = cfg::ESC_MIN_US;
float gOutUs = cfg::ESC_MIN_US;
uint8_t gMode = 0;
uint32_t gLastMs = 0;

void writeUs(float us) {
  us = constrain(us, static_cast<float>(cfg::ESC_MIN_US), static_cast<float>(cfg::ESC_MAX_US));
  pwmWriteUs(gEsc, static_cast<uint16_t>(lroundf(us)));
}

}  // namespace

bool fan::begin() {
  pwmBegin(gEsc, cfg::PIN_ESC, cfg::ESC_MIN_US);
  delay(cfg::ESC_ARM_MS);
  gLastMs = millis();
  return true;
}

void fan::setMode(uint8_t mode) {
  if (mode > 3) mode = 3;
  gMode = mode;
  gTargetUs = cfg::MODE_PULSE_US[mode];
}

uint8_t fan::mode() { return gMode; }

void fan::stopNow() {
  gMode = 0;
  gTargetUs = cfg::ESC_MIN_US;
  gOutUs = cfg::ESC_MIN_US;
  writeUs(gOutUs);
}

void fan::update() {
  const uint32_t now = millis();
  if (now - gLastMs < cfg::FAN_UPDATE_MS) return;
  float dt = (now - gLastMs) / 1000.0f;
  gLastMs = now;
  dt = constrain(dt, 0.0f, 0.25f);

  const float step = cfg::FAN_SLEW_US_PER_S * dt;
  gOutUs += constrain(gTargetUs - gOutUs, -step, step);
  writeUs(gOutUs);
}

uint16_t fan::outputUs() { return static_cast<uint16_t>(lroundf(gOutUs)); }
