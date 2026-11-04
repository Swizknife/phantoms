#pragma once
#include <Arduino.h>

// HC-SR501 backup sensor on PIN_PIR. It only reports "motion", and its own rotation triggers it,
// so readings are taken in a "look": the head is stopped, OUT is allowed to drop, then watched.
namespace pir {

// Plain constants instead of an enum (see the note in follow_me_fan_full.ino).
constexpr int8_t LOOK_BUSY = -1;
constexpr int8_t LOOK_EMPTY = 0;
constexpr int8_t LOOK_PERSON = 1;

void begin();
bool motion();       // raw OUT level
bool warmedUp();     // PIR_WARMUP_MS since boot
void startLook();    // call once the head has stopped
void cancelLook();
int8_t updateLook(); // LOOK_BUSY until done, then LOOK_EMPTY or LOOK_PERSON (once)
bool looking();

}  // namespace pir
