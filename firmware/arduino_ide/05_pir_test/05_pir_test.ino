/*
  TEST 5: HC-SR501 PIR motion sensor -- backup for the MLX90614 (Arduino Uno).
  The main firmware switches to this sensor automatically when no MLX90614 answers at boot.
  No motors needed: USB power is enough for this test.

  Wiring:
    HC-SR501 VCC -> Uno 5V
    HC-SR501 OUT -> Uno D2
    HC-SR501 GND -> Uno GND
  Board settings: trigger jumper on H (repeat), TIME pot fully anticlockwise (~3 s),
  SENS pot in the middle.

  Expected (Serial Monitor, 115200 baud):
    A 60 s warm-up countdown (OUT may flicker during it -- ignore that).
    Then "MOTION" when you wave a hand ~1-3 m in front of the dome, and "clear" ~3 s after
    you stop moving. The on-board LED (L) mirrors OUT.
    If it never shows MOTION: check 5V/GND, the OUT wire on D2, and the SENS pot.
    If it shows MOTION constantly: turn SENS down, keep it away from heaters/sunlight/the fan.
*/

constexpr uint8_t PIR_PIN = 2;
constexpr uint32_t WARMUP_MS = 60000;

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println(F("\n=== HC-SR501 PIR test (D2) ==="));
}

void loop() {
  static bool last = false;
  static uint32_t lastPrintMs = 0;
  static uint32_t motionSinceMs = 0;
  const uint32_t now = millis();
  const bool m = digitalRead(PIR_PIN) == HIGH;
  digitalWrite(LED_BUILTIN, m);

  if (now < WARMUP_MS) {
    if (now - lastPrintMs >= 5000) {
      lastPrintMs = now;
      Serial.print(F("warming up... "));
      Serial.print((WARMUP_MS - now) / 1000);
      Serial.println(F(" s left"));
    }
    last = m;
    return;
  }

  if (m != last) {
    last = m;
    Serial.print(now / 1000.0f, 1);
    if (m) {
      motionSinceMs = now;
      Serial.println(F(" s  MOTION"));
    } else {
      Serial.print(F(" s  clear (was HIGH for "));
      Serial.print((now - motionSinceMs) / 1000.0f, 1);
      Serial.println(F(" s)"));
    }
  }
}
