/*
  Follow-Me Fan -- Arduino Uno (ATmega328P) firmware.

  Flow: full 0..360 deg thermal scan -> find warm groups (people) -> swing the fan head back and
  forth over the arc that covers them -> set the BLDC speed from the person/ambient temperature
  delta. A full rescan happens every TRACK_RESCAN_MS, after LOST_PASSES empty swings, or every
  IDLE_RESCAN_MS while nobody is found (see config.h).

  Motors: only the MG996R (yaw, D9) and the BLDC via the SimonK ESC (D10). There is no tilt.

  PIR backup: if no MLX90614 answers at boot, the HC-SR501 on D2 is used instead. The head then
  scans in PIR_STEP_DEG steps, stopping for a PIR "look" at each one, swings over the arc where
  motion was seen (looking again at each end), and runs the fan at PIR_FAN_MODE.

  RAM is tight (2 KB): all text uses F(), there is no String/printf, and the scan keeps about
  360 bytes of state. `status` prints the free RAM.
*/
#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "fan.h"
#include "pir.h"
#include "scan.h"
#include "thermal.h"
#include "yaw.h"

// Plain constants instead of an enum: the Arduino IDE generates function prototypes before it
// sees types declared in the sketch, so custom types in signatures can break the build.
static const uint8_t S_BOOT = 0;
static const uint8_t S_SCAN_HOME = 1;
static const uint8_t S_SCAN_FWD = 2;
static const uint8_t S_SCAN_BACK = 3;
static const uint8_t S_IDLE = 4;
static const uint8_t S_TRACK = 5;
static const uint8_t S_FAULT = 6;
static const uint8_t S_HALT = 7;  // after `stop` or during `cal`: nothing moves until `scan`
static const uint8_t S_PIR_WARMUP = 8;  // PIR mode: waiting PIR_WARMUP_MS after power-on
static const uint8_t S_PIR_SCAN = 9;    // PIR mode: step 0..360, look at each stop

static uint8_t gState = S_BOOT;
static uint32_t gStateSinceMs = 0;
static uint32_t gLastSensorMs = 0;
static uint32_t gLastSensorOkMs = 0;
static float gAmbientEma = 25.0f;
static bool gAmbientInit = false;
static bool gLogging = false;
static int8_t gManualMode = -1;  // -1 = auto

static float gArcL = 0, gArcR = 0;
static float gDelta = 0;
static uint8_t gAutoMode = 0;
static uint32_t gModeSinceMs = 0;
static uint8_t gLostPasses = 0;
static uint32_t gLastFullScanMs = 0;
static bool gSweeping = false;  // TRACK: true while crossing the arc (samples are recorded)

static bool gUsePir = false;    // set at boot when no MLX90614 answers
static uint8_t gPirStep = 0;    // PIR_SCAN: index of the current stop
static bool gPirFound = false;
static float gPirMinDeg = 0, gPirMaxDeg = 0;

static char gCmd[24];
static uint8_t gCmdLen = 0;

// ---------- small helpers ----------

static void enterState(uint8_t s) {
  gState = s;
  gStateSinceMs = millis();
}

static const __FlashStringHelper *stateName(uint8_t s) {
  switch (s) {
    case S_BOOT: return F("BOOT");
    case S_SCAN_HOME: return F("SCAN_HOME");
    case S_SCAN_FWD: return F("SCAN_FWD");
    case S_SCAN_BACK: return F("SCAN_BACK");
    case S_IDLE: return F("IDLE");
    case S_TRACK: return F("TRACK");
    case S_FAULT: return F("FAULT");
    case S_HALT: return F("HALT");
    case S_PIR_WARMUP: return F("PIR_WARMUP");
    case S_PIR_SCAN: return F("PIR_SCAN");
  }
  return F("?");
}

static int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

static void printF(const __FlashStringHelper *k, float v, uint8_t digits) {
  Serial.print(' ');
  Serial.print(k);
  Serial.print('=');
  Serial.print(v, digits);
}

static void printL(const __FlashStringHelper *k, long v) {
  Serial.print(' ');
  Serial.print(k);
  Serial.print('=');
  Serial.print(v);
}

// ---------- fan mode ----------

static void applyFan() { fan::setMode(gManualMode >= 0 ? static_cast<uint8_t>(gManualMode) : gAutoMode); }

// Auto mode from delta (person Tobj - ambient), with hysteresis so it doesn't chatter at the
// boundaries. A detected person always gets at least mode 1. Changes wait MODE_MIN_DWELL_MS,
// except the very first switch-on from 0.
static void updateAutoMode(float delta) {
  uint8_t next = gAutoMode < 1 ? 1 : gAutoMode;
  if (next == 1 && delta >= cfg::MODE_UP_2) next = 2;
  if (next == 2 && delta >= cfg::MODE_UP_3) next = 3;
  if (next == 3 && delta < cfg::MODE_DOWN_2) next = 2;
  if (next == 2 && delta < cfg::MODE_DOWN_1) next = 1;

  if (next != gAutoMode && (gAutoMode == 0 || millis() - gModeSinceMs >= cfg::MODE_MIN_DWELL_MS)) {
    gAutoMode = next;
    gModeSinceMs = millis();
    Serial.print(F("[mode] auto mode -> "));
    Serial.print(gAutoMode);
    Serial.print(F(" (delta="));
    Serial.print(delta, 2);
    Serial.println(')');
  }
  applyFan();
}

// ---------- sensor ----------

// Reads the sensor (rate-limited) in EVERY state so the fault watchdog and ambient average stay
// alive while idle. Samples go into the scan profile only when `record` is set and the head moves.
// Returns true if a valid reading was taken this call.
static bool pollSensor(bool record) {
  if (gUsePir) return false;  // the PIR is read only during looks
  const uint32_t now = millis();
  if (now - gLastSensorMs < cfg::SENSOR_PERIOD_MS) return false;
  gLastSensorMs = now;

  float tobj, ta;
  if (!thermal::read(tobj, ta)) return false;

  gLastSensorOkMs = now;
  if (!gAmbientInit) { gAmbientEma = ta; gAmbientInit = true; }
  else gAmbientEma += cfg::AMBIENT_EMA_ALPHA * (ta - gAmbientEma);

  const int dir = yaw::direction();
  if (record && dir != 0) scan::addSample(yaw::angle(), dir, tobj);
  if (gLogging) {
    Serial.print(yaw::angle(), 1);
    Serial.print(',');
    Serial.print(tobj, 2);
    Serial.print(',');
    Serial.println(ta, 2);
  }
  return true;
}

static bool sensorFaulted() { return gAmbientInit && (millis() - gLastSensorOkMs > cfg::SENSOR_FAULT_MS); }

// ---------- scan control ----------

static void startFullScan() {
  if (gUsePir && !pir::warmedUp()) {
    yaw::moveTo(cfg::YAW_MIN_DEG);
    enterState(S_PIR_WARMUP);
    return;
  }
  scan::reset();
  pir::cancelLook();
  gLastFullScanMs = millis();
  gSweeping = false;
  yaw::moveTo(cfg::YAW_MIN_DEG);
  enterState(S_SCAN_HOME);
}

// PIR mode: the stepped scan is done. Swing arc = the stops where motion was seen, widened by
// half a step (the person may be between stops) plus the usual margin.
static void finishPirScan() {
  gLastFullScanMs = millis();  // count the rescan interval from the end of this long scan
  if (gPirFound) {
    gArcL = constrain(gPirMinDeg - cfg::PIR_STEP_DEG * 0.5f - cfg::SWING_MARGIN_DEG, cfg::YAW_MIN_DEG, cfg::YAW_MAX_DEG);
    gArcR = constrain(gPirMaxDeg + cfg::PIR_STEP_DEG * 0.5f + cfg::SWING_MARGIN_DEG, cfg::YAW_MIN_DEG, cfg::YAW_MAX_DEG);
    gLostPasses = 0;
    Serial.print(F("[pir scan] person, arc=["));
    Serial.print(gArcL, 1);
    Serial.print(',');
    Serial.print(gArcR, 1);
    Serial.println(']');
    gAutoMode = cfg::PIR_FAN_MODE;
    applyFan();
    gSweeping = false;
    yaw::moveTo(gArcL);  // comes back anticlockwise from 360
    enterState(S_TRACK);
  } else {
    Serial.println(F("[pir scan] no motion seen"));
    gAutoMode = 0;
    applyFan();
    yaw::moveTo(cfg::YAW_MIN_DEG);
    enterState(S_IDLE);
  }
}

static void printStatus() {
  Serial.print(F("state="));
  Serial.print(stateName(gState));
  Serial.print(gUsePir ? F(" sensor=PIR") : F(" sensor=MLX"));
  printF(F("angle"), yaw::angle(), 1);
  printL(F("dir"), yaw::direction());
  printL(F("fanUs"), fan::outputUs());
  printL(F("mode"), fan::mode());
  printL(F("auto"), gAutoMode);
  printL(F("manual"), gManualMode);
  printF(F("ambient"), gAmbientEma, 2);
  printF(F("delta"), gDelta, 2);
  printF(F("arcL"), gArcL, 1);
  printF(F("arcR"), gArcR, 1);
  printL(F("cal"), yaw::isCalibrated());
  printL(F("stopUs"), yaw::stopUs());
  printF(F("wFwd"), yaw::omegaFwd(), 2);
  printF(F("wBack"), yaw::omegaBack(), 2);
  printL(F("freeRam"), freeRam());
  Serial.println();
}

// ---------- calibration ----------

static void drainSerial() {
  delay(5);
  while (Serial.available()) Serial.read();
}

static char waitKey() {
  while (!Serial.available()) delay(10);
  const char c = Serial.read();
  drainSerial();
  return c;
}

static float waitNumber() {
  while (!Serial.available()) delay(10);
  const float v = Serial.parseFloat();
  drainSerial();
  return v;
}

// Interactive calibration: find the stop pulse, then measure deg/s each direction.
static void runCalibration() {
  enterState(S_HALT);  // keep the state machine from moving the head meanwhile
  yaw::spin(0);

  Serial.println(F("--- Calibration: finding stop pulse ---"));
  Serial.println(F("Type 'u'/'d' to nudge the trial stop pulse +/-5us, 's' when the shaft is still, 'q' to abort."));
  uint16_t trial = cfg::SERVO_STOP_US_DEFAULT;
  while (true) {
    yaw::writeRawPulse(trial);
    Serial.print(F("trial stop = "));
    Serial.print(trial);
    Serial.println(F(" us"));
    const char c = waitKey();
    if (c == 'u') trial += 5;
    else if (c == 'd') trial -= 5;
    else if (c == 's') break;
    else if (c == 'q') { Serial.println(F("Calibration aborted.")); return; }
  }
  const uint16_t stopUs = trial;

  Serial.print(F("Spinning FORWARD for "));
  Serial.print(cfg::CAL_SPIN_MS);
  Serial.println(F(" ms. Watch the fan head and note the total degrees turned."));
  Serial.println(F("It must turn CLOCKWISE (seen from above). If not, set YAW_REVERSE = true in config.h and re-upload."));
  yaw::writeRawPulse(yaw::pulseFor(stopUs, 1));
  delay(cfg::CAL_SPIN_MS);
  yaw::writeRawPulse(stopUs);
  Serial.println(F("Enter degrees turned (forward):"));
  const float degFwd = waitNumber();

  Serial.print(F("Spinning BACK for "));
  Serial.print(cfg::CAL_SPIN_MS);
  Serial.println(F(" ms. Note the total degrees turned."));
  yaw::writeRawPulse(yaw::pulseFor(stopUs, -1));
  delay(cfg::CAL_SPIN_MS);
  yaw::writeRawPulse(stopUs);
  Serial.println(F("Enter degrees turned (back):"));
  const float degBack = waitNumber();

  const float wFwd = fabsf(degFwd) / (cfg::CAL_SPIN_MS / 1000.0f);
  const float wBack = fabsf(degBack) / (cfg::CAL_SPIN_MS / 1000.0f);
  if (wFwd < 1.0f || wBack < 1.0f) {
    Serial.println(F("Measured speed too low; calibration rejected. Check wiring/servo."));
    return;
  }
  yaw::saveCalibration(stopUs, wFwd, wBack);
  yaw::setAngle(0);
  Serial.print(F("Saved to EEPROM: stopUs="));
  Serial.print(stopUs);
  Serial.print(F(" wFwd="));
  Serial.print(wFwd, 2);
  Serial.print(F(" deg/s wBack="));
  Serial.print(wBack, 2);
  Serial.println(F(" deg/s"));
  Serial.println(F("Point the head at its start position (angle 0), then type 'scan'."));
}

// ---------- serial commands ----------

static void execute(const char *c) {
  if (!strcmp_P(c, PSTR("status"))) {
    printStatus();
  } else if (!strcmp_P(c, PSTR("cal"))) {
    runCalibration();
  } else if (!strcmp_P(c, PSTR("scan"))) {
    if (gState == S_HALT) gManualMode = -1;  // `scan` resumes after `stop`
    startFullScan();
  } else if (!strcmp_P(c, PSTR("profile"))) {
    scan::printProfile(gAmbientEma);
  } else if (!strcmp_P(c, PSTR("log on"))) {
    gLogging = true;
    Serial.println(F("logging ON (angle,tobj,ta)"));
  } else if (!strcmp_P(c, PSTR("log off"))) {
    gLogging = false;
    Serial.println(F("logging OFF"));
  } else if (!strncmp_P(c, PSTR("mode "), 5)) {
    const int m = atoi(c + 5);
    gManualMode = (m <= 0) ? -1 : constrain(m, 1, 3);
    applyFan();
    Serial.print(F("manual mode = "));
    Serial.print(gManualMode);
    Serial.println(F(" (-1 means auto)"));
  } else if (!strcmp_P(c, PSTR("stop"))) {
    fan::stopNow();
    yaw::spin(0);
    gManualMode = 0;
    gAutoMode = 0;
    enterState(S_HALT);
    Serial.println(F("Stopped. Fan at minimum, servo halted. Type 'scan' to resume."));
  } else {
    Serial.println(F("commands: status | cal | scan | profile | log on|off | mode 0-3 | stop"));
  }
}

// Non-blocking line reader into a fixed buffer (no String, no heap).
static void handleSerial() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (gCmdLen == 0) continue;
      gCmd[gCmdLen] = 0;
      gCmdLen = 0;
      execute(gCmd);
      return;  // a command (e.g. cal) may have consumed serial input itself
    }
    if (gCmdLen < sizeof(gCmd) - 1) gCmd[gCmdLen++] = c;
  }
}

// ---------- Arduino entry points ----------

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== Follow-Me Fan firmware (Arduino Uno) ==="));
  yaw::begin();
  scan::reset();

  if (!thermal::begin()) {
    gUsePir = true;
    pir::begin();
    Serial.println(F("MLX90614 not found at 0x5A -> using the HC-SR501 PIR on D2 instead."));
    Serial.println(F("(No PIR fitted? Then check the MLX90614 wiring/pull-ups and reset.)"));
    Serial.print(F("PIR warm-up: scanning starts "));
    Serial.print(cfg::PIR_WARMUP_MS / 1000);
    Serial.println(F(" s after power-on."));
  }
  if (!yaw::isCalibrated()) {
    Serial.println(F("Servo not calibrated yet. Run 'cal' before autonomous operation."));
  }

  Serial.println(F("Arming ESC (blades off!)..."));
  fan::begin();
  Serial.println(F("Ready. Commands: status | cal | scan | profile | log on|off | mode 0-3 | stop"));

  gLastSensorOkMs = millis();
  startFullScan();
}

void loop() {
  handleSerial();
  yaw::update();
  fan::update();

  const bool record = gState == S_SCAN_FWD || gState == S_SCAN_BACK || (gState == S_TRACK && gSweeping);
  const bool gotRead = pollSensor(record);

  if (gState == S_FAULT) {
    if (gotRead) {
      Serial.println(F("[fault] sensor recovered -> rescanning"));
      startFullScan();
    }
    return;
  }
  if (gState != S_HALT && sensorFaulted()) {
    Serial.println(F("[fault] sensor read timeout -> stopping fan and servo"));
    fan::stopNow();
    yaw::spin(0);
    gAutoMode = 0;
    enterState(S_FAULT);
    return;
  }

  switch (gState) {
    case S_BOOT:
      startFullScan();
      break;

    case S_HALT:
      break;

    case S_PIR_WARMUP:
      if (pir::warmedUp()) startFullScan();
      break;

    case S_SCAN_HOME:
      // Homes the head to angle 0 before the real sweep; nothing is recorded while homing.
      if (!yaw::isMoving()) {
        if (gUsePir) {
          gPirStep = 0;
          gPirFound = false;
          enterState(S_PIR_SCAN);
        } else {
          yaw::moveTo(cfg::YAW_MAX_DEG);
          enterState(S_SCAN_FWD);
        }
      }
      break;

    case S_PIR_SCAN: {
      // Clockwise in PIR_STEP_DEG steps: stop, look, step on. Ends at 360.
      if (yaw::isMoving()) break;
      if (!pir::looking()) { pir::startLook(); break; }
      const int8_t r = pir::updateLook();
      if (r == pir::LOOK_BUSY) break;

      const float here = gPirStep * cfg::PIR_STEP_DEG;
      if (r == pir::LOOK_PERSON) {
        Serial.print(F("[pir scan] motion at "));
        Serial.println(here, 0);
        if (!gPirFound) { gPirMinDeg = gPirMaxDeg = here; gPirFound = true; }
        else { gPirMinDeg = min(gPirMinDeg, here); gPirMaxDeg = max(gPirMaxDeg, here); }
      }
      gPirStep++;
      const float next = gPirStep * cfg::PIR_STEP_DEG;
      if (next <= cfg::YAW_MAX_DEG + 0.1f) yaw::moveTo(next);
      else finishPirScan();
      break;
    }

    case S_SCAN_FWD:
      if (!yaw::isMoving()) {
        yaw::moveTo(cfg::YAW_MIN_DEG);
        enterState(S_SCAN_BACK);
      }
      break;

    case S_SCAN_BACK:
      // Sweeping back over the same bins lets forward/back lag average out in scan::analyze.
      if (!yaw::isMoving()) {
        const scan::Result res = scan::analyze(gAmbientEma, gLogging);
        if (res.valid) {
          gArcL = res.arcL;
          gArcR = res.arcR;
          gDelta = res.delta;
          gLostPasses = 0;
          Serial.print(F("[scan] "));
          Serial.print(res.numPeople);
          Serial.print(F(" people, arc=["));
          Serial.print(gArcL, 1);
          Serial.print(',');
          Serial.print(gArcR, 1);
          Serial.print(F("] delta="));
          Serial.println(gDelta, 2);
          updateAutoMode(gDelta);
          gSweeping = false;
          yaw::moveTo(gArcL);  // position at the arc start; the first swing begins from there
          enterState(S_TRACK);
        } else {
          Serial.println(F("[scan] no person found"));
          gAutoMode = 0;
          applyFan();
          yaw::moveTo(cfg::YAW_MIN_DEG);
          enterState(S_IDLE);
        }
      }
      break;

    case S_IDLE:
      if (!yaw::isMoving() && millis() - gStateSinceMs > cfg::IDLE_RESCAN_MS) startFullScan();
      break;

    case S_TRACK:
      if (yaw::isMoving()) break;

      if (gUsePir && gSweeping) {  // at one end of the arc: stop and look before swinging back
        if (!pir::looking()) { pir::startLook(); break; }
        const int8_t r = pir::updateLook();
        if (r == pir::LOOK_BUSY) break;
        gLostPasses = (r == pir::LOOK_PERSON) ? 0 : gLostPasses + 1;

        if (gLostPasses >= cfg::PIR_LOST_PASSES) {
          Serial.println(F("[track] no motion for a while -> rescanning"));
          gAutoMode = 0;
          applyFan();
          startFullScan();
          break;
        }
        if (millis() - gLastFullScanMs > cfg::PIR_TRACK_RESCAN_MS) {
          Serial.println(F("[track] periodic full rescan"));
          startFullScan();
          break;
        }
      } else if (gSweeping) {  // just finished one swing across the arc
        const scan::Result res = scan::analyze(gAmbientEma);
        if (res.valid) {
          gArcL = res.arcL;
          gArcR = res.arcR;
          gDelta = res.delta;
          gLostPasses = 0;
          updateAutoMode(gDelta);
        } else {
          gLostPasses++;
        }

        if (gLostPasses >= cfg::LOST_PASSES) {
          Serial.println(F("[track] person lost -> rescanning"));
          gAutoMode = 0;
          applyFan();
          startFullScan();
          break;
        }
        if (millis() - gLastFullScanMs > cfg::TRACK_RESCAN_MS) {
          Serial.println(F("[track] periodic full rescan"));
          startFullScan();
          break;
        }
      }

      {
        // Swing to the opposite end. Only this direction's samples inside the arc are cleared, so
        // the rest of the profile (baseline from the full scan) stays valid for analyze().
        const bool toRight = yaw::angle() < gArcR - 0.5f;
        if (!gUsePir) scan::clearRange(gArcL, gArcR, toRight ? 1 : -1);
        gSweeping = true;
        yaw::moveTo(toRight ? gArcR : gArcL);
      }
      break;
  }
}
