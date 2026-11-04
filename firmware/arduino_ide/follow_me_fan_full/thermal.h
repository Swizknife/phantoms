#pragma once

namespace thermal {
bool begin();
// Object and ambient temperature in degrees C. False on I2C/PEC/error-flag failure.
bool read(float &tobj, float &ta);
}  // namespace thermal
