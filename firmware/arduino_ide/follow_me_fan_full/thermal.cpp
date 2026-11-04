#include "thermal.h"

#include <Wire.h>

#include "config.h"

namespace {

constexpr uint8_t REG_TA   = 0x06;
constexpr uint8_t REG_TOBJ = 0x07;

uint8_t crc8(uint8_t crc, uint8_t data) {
  crc ^= data;
  for (uint8_t i = 0; i < 8; i++) {
    crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
  }
  return crc;
}

bool readReg(uint8_t reg, float &outC) {
  Wire.beginTransmission(cfg::MLX_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // SMBus repeated start
  if (Wire.requestFrom(cfg::MLX_ADDR, static_cast<uint8_t>(3)) != 3) return false;

  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  const uint8_t pec = Wire.read();

  uint8_t c = 0;
  c = crc8(c, cfg::MLX_ADDR << 1);
  c = crc8(c, reg);
  c = crc8(c, (cfg::MLX_ADDR << 1) | 1);
  c = crc8(c, lo);
  c = crc8(c, hi);
  if (c != pec) return false;

  const uint16_t raw = (static_cast<uint16_t>(hi) << 8) | lo;
  if (raw & 0x8000) return false;  // sensor error flag
  outC = raw * 0.02f - 273.15f;
  return true;
}

}  // namespace

bool thermal::begin() {
  Wire.begin();
  Wire.setClock(cfg::I2C_HZ);
  Wire.setWireTimeout(cfg::I2C_TIMEOUT_US, true);
  // Wire.begin() enables the ATmega's internal pull-ups to 5 V. The MLX90614 is a 3.3 V part,
  // so switch them off: only 3V3 pull-ups may pull the bus up (the GY-906's own resistors, or
  // the external 4.7 k ones if the board has none).
  digitalWrite(cfg::PIN_SDA, LOW);
  digitalWrite(cfg::PIN_SCL, LOW);
  // A few tries: a single glitch at power-up must not drop the firmware into PIR fallback.
  for (uint8_t i = 0; i < 3; i++) {
    float ta;
    if (readReg(REG_TA, ta)) return true;
    delay(100);
  }
  return false;
}

bool thermal::read(float &tobj, float &ta) {
  float o, a;
  if (!readReg(REG_TOBJ, o) || !readReg(REG_TA, a)) return false;
  tobj = o;
  ta = a;
  return true;
}
