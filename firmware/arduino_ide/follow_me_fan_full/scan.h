#pragma once
#include <Arduino.h>

#include "config.h"

// Thermal profile over 0..360 deg in 10 deg bins, kept separately for forward and
// back sweeps so sensor lag cancels when the two are averaged.
// Memory is deliberately small (about 360 bytes of state) because the Uno has 2 KB of RAM.
namespace scan {

struct Result {
  bool valid;          // at least one person found
  float baseline;      // median temperature of the whole ring
  uint8_t numGroups;   // warm groups found
  uint8_t numPeople;   // groups whose peak is >= PERSON_DELTA_C above ambient
  float arcL, arcR;    // swing limits covering all people
  float delta;         // max (peak - ambient) over people
};

void reset();
// Forget one direction's samples inside [fromDeg, toDeg] so the next sweep refreshes them.
// The other direction (and every bin outside the range) keeps its data.
void clearRange(float fromDeg, float toDeg, int dir);
void addSample(float angleDeg, int dir, float tobj);
// verbose = print one line per warm group to Serial.
Result analyze(float ambient, bool verbose = false);
void printProfile(float ambient);

}  // namespace scan
