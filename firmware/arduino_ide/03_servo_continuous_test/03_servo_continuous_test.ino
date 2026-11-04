/*
  TEST 3: MG996R continuous-rotation servo (Arduino Uno).
  Goal: find the "stop" pulse width (where the shaft does not turn), and confirm it spins in both
  directions around that point. This also confirms whether your servo is really the
  continuous-rotation type.

  Wiring:
    Servo Orange (signal) -> Uno D9
    Servo Red (+5V)       -> LM2596 OUT+ (5.05V), NOT the Uno 5V pin
    Servo Brown (GND)     -> common star ground (also tied to Uno GND)
    1000uF capacitor across Red/Brown at the servo connector

  How to use:
    Open Serial Monitor at 115200 baud, line ending = Newline.
    Type a pulse width in microseconds (900-2100) and press Enter.
    Example: type "1500" and press Enter to try 1500us.

    Try values around 1500 until the shaft stops turning completely. That value is your STOP
    pulse. Then try ~60us below it (stop-60) -> should spin one way, and ~60us above it
    (stop+60) -> should spin the other way.

    If the shaft moves to an angle and HOLDS instead of spinning continuously, this is NOT a
    continuous-rotation servo -- stop and report this back before continuing, the final
    firmware assumes continuous rotation.

    DIRECTION: the head must sweep CLOCKWISE (seen from above) first. Note which way stop-60
    turns it:
      stop-60 turns CLOCKWISE     -> leave YAW_REVERSE = false in follow_me_fan_full/config.h
      stop-60 turns ANTICLOCKWISE -> set   YAW_REVERSE = true
*/

#include <Servo.h>

constexpr uint8_t SERVO_PIN = 9;

Servo servo;

void setup() {
  Serial.begin(115200);
  servo.writeMicroseconds(1500);
  servo.attach(SERVO_PIN, 500, 2500);
  Serial.println(F("Servo test ready. Type a pulse width in microseconds (900-2100) and press Enter."));
}

void loop() {
  if (Serial.available()) {
    const long us = Serial.parseInt();
    while (Serial.available()) Serial.read();  // clear leftover newline
    if (us >= 900 && us <= 2100) {
      servo.writeMicroseconds((int)us);
      Serial.print(F("Pulse set to "));
      Serial.print(us);
      Serial.println(F(" us  (below stop = forward: should be CLOCKWISE, else YAW_REVERSE = true)"));
    } else if (us != 0) {
      Serial.println(F("Out of range. Enter a value between 900 and 2100."));
    }
  }
}
