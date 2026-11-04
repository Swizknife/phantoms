#include "yaw.h"

#include <EEPROM.h>
#include <math.h>

#include "config.h"
#include "pwm.h"

namespace {

// Calibration lives in EEPROM address 0. The magic number replaces a separate "calibrated" flag.
constexpr uint16_t CAL_MAGIC = 0xF1A5;
struct CalData {
  uint16_t magic;
  uint16_t stopUs;
  float wFwd;
  float wBack;
};

Servo gServo;
uint16_t gStopUs = cfg::SERVO_STOP_US_DEFAULT;
float gOmegaFwd = cfg::OMEGA_DEFAULT_DEG_S;
float gOmegaBack = cfg::OMEGA_DEFAULT_DEG_S;
bool gCalibrated = false;

float gAngle = 0.0f;
int gDir = 0;
bool gHasTarget = false;
float gTarget = 0.0f;
uint32_t gLastUs = 0;

void writeUs(uint16_t us) { pwmWriteUs(gServo, us); }

void applyDir(int dir) {
  gDir = dir;
  writeUs(dir > 0 ? yaw::fwdPulse() : dir < 0 ? yaw::backPulse() : gStopUs);
}

}  // namespace

void yaw::begin() {
  CalData c;
  EEPROM.get(0, c);
  if (c.magic == CAL_MAGIC && c.stopUs >= 1000 && c.stopUs <= 2000 && c.wFwd >= 1.0f &&
      c.wBack >= 1.0f) {
    gCalibrated = true;
    gStopUs = c.stopUs;
    gOmegaFwd = c.wFwd;
    gOmegaBack = c.wBack;
  }

  pwmBegin(gServo, cfg::PIN_SERVO, gStopUs);  // first pulse = stop, so the head never twitches at boot
  applyDir(0);
  gLastUs = micros();
}

void yaw::update() {
  const uint32_t now = micros();
  const float dt = (now - gLastUs) * 1e-6f;
  gLastUs = now;
  if (gDir == 0) return;

  gAngle += (gDir > 0) ? gOmegaFwd * dt : -gOmegaBack * dt;

  bool stop = false;
  if (gAngle >= cfg::YAW_MAX_DEG) { gAngle = cfg::YAW_MAX_DEG; stop = true; }
  if (gAngle <= cfg::YAW_MIN_DEG) { gAngle = cfg::YAW_MIN_DEG; stop = true; }
  if (gHasTarget && ((gDir > 0 && gAngle >= gTarget) || (gDir < 0 && gAngle <= gTarget))) stop = true;

  if (stop) {
    gHasTarget = false;
    applyDir(0);
  }
}

void yaw::spin(int dir) {
  update();
  gHasTarget = false;
  applyDir(dir);
}

void yaw::moveTo(float deg) {
  update();
  deg = constrain(deg, cfg::YAW_MIN_DEG, cfg::YAW_MAX_DEG);
  if (fabsf(deg - gAngle) < 0.5f) {
    gHasTarget = false;
    applyDir(0);
    return;
  }
  gTarget = deg;
  gHasTarget = true;
  applyDir(deg > gAngle ? 1 : -1);
}

bool yaw::isMoving() { return gDir != 0; }
int yaw::direction() { return gDir; }
float yaw::angle() { return gAngle; }
void yaw::setAngle(float deg) { gAngle = constrain(deg, cfg::YAW_MIN_DEG, cfg::YAW_MAX_DEG); }

void yaw::writeRawPulse(uint16_t us) {
  update();
  gHasTarget = false;
  gDir = 0;
  writeUs(us);
}

uint16_t yaw::stopUs() { return gStopUs; }
uint16_t yaw::pulseFor(uint16_t stopUs, int dir) {
  if (dir == 0) return stopUs;
  const bool lower = (dir > 0) != cfg::YAW_REVERSE;  // forward = stop - offset unless reversed
  return lower ? stopUs - cfg::SERVO_SCAN_OFFSET_US : stopUs + cfg::SERVO_SCAN_OFFSET_US;
}
uint16_t yaw::fwdPulse() { return pulseFor(gStopUs, 1); }
uint16_t yaw::backPulse() { return pulseFor(gStopUs, -1); }
float yaw::omegaFwd() { return gOmegaFwd; }
float yaw::omegaBack() { return gOmegaBack; }
bool yaw::isCalibrated() { return gCalibrated; }

void yaw::saveCalibration(uint16_t stopUs, float omegaFwd, float omegaBack) {
  gStopUs = stopUs;
  gOmegaFwd = omegaFwd;
  gOmegaBack = omegaBack;
  gCalibrated = true;
  const CalData c = {CAL_MAGIC, stopUs, omegaFwd, omegaBack};
  EEPROM.put(0, c);  // put() only rewrites bytes that changed
}
