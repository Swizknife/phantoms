#include "pir.h"

#include "config.h"

namespace {

constexpr uint8_t PH_IDLE = 0;
constexpr uint8_t PH_SETTLE = 1;
constexpr uint8_t PH_WATCH = 2;

uint8_t gPhase = PH_IDLE;
uint32_t gPhaseSinceMs = 0;
bool gSeen = false;

}  // namespace

void pir::begin() { pinMode(cfg::PIN_PIR, INPUT); }

bool pir::motion() { return digitalRead(cfg::PIN_PIR) == HIGH; }

bool pir::warmedUp() { return millis() >= cfg::PIR_WARMUP_MS; }

void pir::startLook() {
  gPhase = PH_SETTLE;
  gPhaseSinceMs = millis();
  gSeen = false;
}

void pir::cancelLook() { gPhase = PH_IDLE; }

bool pir::looking() { return gPhase != PH_IDLE; }

int8_t pir::updateLook() {
  const uint32_t t = millis() - gPhaseSinceMs;
  switch (gPhase) {
    case PH_SETTLE:
      // OUT stays HIGH for the hold time after the head's own motion triggered it. If it is
      // still HIGH after SETTLE_MAX, something really is moving, and the watch below will see it.
      if (t >= cfg::PIR_SETTLE_MIN_MS && (!motion() || t >= cfg::PIR_SETTLE_MAX_MS)) {
        gPhase = PH_WATCH;
        gPhaseSinceMs = millis();
      }
      return LOOK_BUSY;
    case PH_WATCH:
      if (motion()) gSeen = true;
      if (t < cfg::PIR_WATCH_MS) return LOOK_BUSY;
      gPhase = PH_IDLE;
      return gSeen ? LOOK_PERSON : LOOK_EMPTY;
  }
  return LOOK_EMPTY;
}
