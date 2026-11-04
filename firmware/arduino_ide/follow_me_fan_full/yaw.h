#pragma once
#include <Arduino.h>

// Continuous-rotation servo with open-loop angle estimate (omega x time).
// dir +1 = forward (angle increases), -1 = back, 0 = stopped.
namespace yaw {
void begin();
void update();                 // call every loop; integrates angle and stops at target/limits
void spin(int dir);
void moveTo(float deg);
bool isMoving();
int direction();
float angle();
void setAngle(float deg);
void writeRawPulse(uint16_t us);  // calibration/jog only; does not change the angle estimate

uint16_t stopUs();
uint16_t fwdPulse();
uint16_t backPulse();
uint16_t pulseFor(uint16_t stopUs, int dir);  // honours cfg::YAW_REVERSE
float omegaFwd();
float omegaBack();
bool isCalibrated();
void saveCalibration(uint16_t stopUs, float omegaFwd, float omegaBack);
}  // namespace yaw
