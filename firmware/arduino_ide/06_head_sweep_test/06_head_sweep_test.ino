/*
  TEST 6: full head sweep -- MG996R turns the fan head CLOCKWISE 0 -> 360 deg, pauses, then
  ANTICLOCKWISE 360 -> 0 deg (Arduino Uno). Run this after test 3, with the head (fan + sensor)
  mounted, to check the motion, the timing and that the cables don't pull tight.

  Wiring: same as test 3 (servo orange -> D9, red -> LM2596 OUT+ 5.05V, brown -> star ground,
  1000uF across red/brown). 12V adapter on, ESC may stay connected (it is not driven here).

  Before uploading, set the values below:
    STOP_US      the stop pulse found in test 3
    OMEGA_*      head speed in deg/s; start with 30 and correct it (see below)
    YAW_REVERSE  false if stop-60 turned the head clockwise in test 3, true if anticlockwise
  Use the same values the main firmware will use (its `cal` command measures OMEGA for you).

  How to use (Serial Monitor, 115200 baud, line ending = Newline):
    g  -> one full sweep: CW 0->360, 1 s pause, CCW 360->0
    s  -> stop immediately
  Mark the head's start position. If after the CW leg it is short of / past a full turn, adjust
  OMEGA_FWD (e.g. it only made 300 deg: OMEGA_FWD = 30 * 300 / 360 = 25). Same for OMEGA_BACK.
*/

#include <Servo.h>

constexpr uint8_t SERVO_PIN = 9;
constexpr uint16_t STOP_US = 1500;
constexpr uint16_t OFFSET_US = 60;      // same as SERVO_SCAN_OFFSET_US in the main firmware
constexpr float OMEGA_FWD = 30.0f;      // deg/s clockwise
constexpr float OMEGA_BACK = 30.0f;     // deg/s anticlockwise
constexpr bool YAW_REVERSE = false;
constexpr float SWEEP_DEG = 360.0f;
constexpr uint32_t PAUSE_MS = 1000;

constexpr uint8_t PH_IDLE = 0;
constexpr uint8_t PH_CW = 1;
constexpr uint8_t PH_PAUSE = 2;
constexpr uint8_t PH_CCW = 3;

Servo servo;
uint8_t phase = PH_IDLE;
uint32_t phaseStartMs = 0;
uint32_t lastPrintMs = 0;

uint16_t pulseFor(int dir) {
  if (dir == 0) return STOP_US;
  const bool lower = (dir > 0) != YAW_REVERSE;
  return lower ? STOP_US - OFFSET_US : STOP_US + OFFSET_US;
}

void enterPhase(uint8_t p) {
  phase = p;
  phaseStartMs = millis();
  servo.writeMicroseconds(pulseFor(p == PH_CW ? 1 : p == PH_CCW ? -1 : 0));
}

void setup() {
  Serial.begin(115200);
  servo.writeMicroseconds(STOP_US);  // first pulse = stop, no twitch at boot
  servo.attach(SERVO_PIN, 500, 2500);
  Serial.println(F("\n=== Head sweep test: 'g' = CW 0->360 then CCW back, 's' = stop ==="));
}

void loop() {
  if (Serial.available()) {
    const char c = Serial.read();
    if (c == 'g' && phase == PH_IDLE) {
      Serial.println(F("CW 0 -> 360 ..."));
      enterPhase(PH_CW);
    } else if (c == 's') {
      enterPhase(PH_IDLE);
      Serial.println(F("STOPPED"));
    }
  }

  const uint32_t t = millis() - phaseStartMs;
  const uint32_t cwMs = (uint32_t)(SWEEP_DEG / OMEGA_FWD * 1000.0f);
  const uint32_t ccwMs = (uint32_t)(SWEEP_DEG / OMEGA_BACK * 1000.0f);

  if (phase == PH_CW || phase == PH_CCW) {
    if (millis() - lastPrintMs >= 1000) {
      lastPrintMs = millis();
      const float deg = (phase == PH_CW) ? OMEGA_FWD * t / 1000.0f : SWEEP_DEG - OMEGA_BACK * t / 1000.0f;
      Serial.print(F("  angle ~ "));
      Serial.println(constrain(deg, 0.0f, SWEEP_DEG), 0);
    }
  }

  if (phase == PH_CW && t >= cwMs) {
    Serial.println(F("at 360 (one full turn?) -- pausing"));
    enterPhase(PH_PAUSE);
  } else if (phase == PH_PAUSE && t >= PAUSE_MS) {
    Serial.println(F("CCW 360 -> 0 ..."));
    enterPhase(PH_CCW);
  } else if (phase == PH_CCW && t >= ccwMs) {
    enterPhase(PH_IDLE);
    Serial.println(F("back at 0 (start mark?). Type 'g' to repeat."));
  }
}
