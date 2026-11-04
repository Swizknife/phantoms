/*
  TEST 4: SimonK 30A ESC + A2212 1000KV BLDC fan motor (Arduino Uno).

  !!! SAFETY: REMOVE THE FAN BLADES BEFORE RUNNING THIS TEST. !!!

  Wiring:
    ESC Thin White (signal) -> Uno D10
    ESC Thin Black (signal GND) -> common star ground (also tied to Uno GND)
    ESC Thick Red  -> 12V adapter (+)
    ESC Thick Black -> common star ground
    ESC Thin RED (BEC +5V) -> NOT CONNECTED, taped off/insulated
    ESC Blue phase wires (U,V,W) -> the 3 A2212 wires, any order (swap any two to reverse)
  Power: the 12V adapter only. A USB power bank cannot run the ESC/BLDC.

  How to use:
    Power on. The ESC arms for 3 seconds (you should hear beeps).
    Open Serial Monitor at 115200 baud, line ending = Newline.
    Type a speed 0-100 and press Enter. The motor should ramp to that speed smoothly over about
    2.5 seconds for a full 0->100 change.

    The output is capped at 1350us (35% of the ESC's full range) to protect the shared 12V 5A
    supply. Type "0" to return to idle/stop.
*/

#include <Servo.h>
#include <math.h>

constexpr uint8_t ESC_PIN = 10;

constexpr uint16_t ESC_MIN_US = 1000;
constexpr uint16_t ESC_MAX_US = 1350;  // hard cap, see bldc_speed_control_handoff.md
constexpr float SLEW_PERCENT_PER_SEC = 40.0f;
constexpr uint32_t UPDATE_MS = 20;

Servo esc;
static float targetPercent = 0.0f;
static float outputPercent = 0.0f;
static uint32_t lastUpdateMs = 0;

void setup() {
  Serial.begin(115200);

  Serial.println(F("Arming ESC (BLADES MUST BE OFF)..."));
  esc.writeMicroseconds(ESC_MIN_US);  // first pulse = minimum throttle, never the 1500 us default
  esc.attach(ESC_PIN, 500, 2500);
  delay(3000);
  lastUpdateMs = millis();
  Serial.println(F("Armed. Type a speed 0-100 and press Enter."));
}

void loop() {
  if (Serial.available()) {
    const long v = Serial.parseInt();
    while (Serial.available()) Serial.read();
    targetPercent = constrain((float)v, 0.0f, 100.0f);
    Serial.print(F("Target speed set to "));
    Serial.print(targetPercent, 0);
    Serial.println('%');
  }

  const uint32_t now = millis();
  if (now - lastUpdateMs < UPDATE_MS) return;
  float dt = (now - lastUpdateMs) / 1000.0f;
  lastUpdateMs = now;
  dt = constrain(dt, 0.0f, 0.25f);

  const float maxStep = SLEW_PERCENT_PER_SEC * dt;
  outputPercent += constrain(targetPercent - outputPercent, -maxStep, maxStep);

  const uint16_t pulseUs = ESC_MIN_US + (uint16_t)lroundf((ESC_MAX_US - ESC_MIN_US) * (outputPercent / 100.0f));
  esc.writeMicroseconds(pulseUs);
}
