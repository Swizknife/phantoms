# Arduino Uno BLDC Speed Control Handoff

## Purpose

This handoff describes how the Arduino Uno drives a SimonK 30 A ESC and an A2212 1000 KV BLDC motor. A numeric speed request from 0 to 100 is converted into the RC-style pulse the ESC expects. Angle is intentionally excluded from this document.

The maintained code is not pasted here any more (the old copy used the ESP32 LEDC API and would no longer compile on the Uno). Use:

- **Bench test:** [`firmware/arduino_ide/04_esc_fan_ramp_test/04_esc_fan_ramp_test.ino`](firmware/arduino_ide/04_esc_fan_ramp_test/04_esc_fan_ramp_test.ino) — type a speed 0-100 in the Serial Monitor.
- **Real firmware:** [`firmware/arduino_ide/follow_me_fan_full/fan.cpp`](firmware/arduino_ide/follow_me_fan_full/fan.cpp) — `fan::setMode()` / `fan::update()`.

## Decisions

- The tracking/control system may produce speed and angle, but this version uses **speed only**.
- In the bench test, speed is a percentage from 0 to 100; values below 0 become 0 and above 100 become 100. The full firmware uses three fixed modes (off, 1, 2, 3) instead.
- ESC output is limited to 1000–1350 microseconds. Relative to a nominal 1000–2000 microsecond ESC range, 1350 microseconds is a 35% command cap.
- Output changes are ramped smoothly at 40 percentage points per second (140 µs/s), with an update every 20 ms.
- **The signal pin is Uno D10**, driven with the Arduino `Servo` library (Timer1), which generates the 50 Hz pulse train. The first pulse is written *before* the pin is attached, so the ESC never sees the library's 1500 µs default (which would be a half-throttle command at power-up).

## How the speed-to-pulse conversion works

The input percentage is clamped to 0–100, then mapped linearly to the allowed ESC pulse range:

| Requested speed | ESC pulse target |
|---:|---:|
| 0% | 1000 µs (minimum/stop command) |
| 50% | about 1175 µs |
| 100% | 1350 µs (configured cap) |

At 50 Hz, one period is 20 ms. This is a servo-style ESC control signal; it is not the high-frequency PWM used to switch motor phases. The ESC performs motor commutation.

The slew limiter moves the output toward each new target in small steps, so a 0-to-100% change takes about 2.5 seconds. Tune `SLEW_PERCENT_PER_SEC` in the test sketch, or `FAN_SLEW_US_PER_S` in `config.h` for the full firmware.

## Setup and use

1. Wire the ESC white signal wire to **Uno D10** and the ESC thin black wire to the star ground (also tied to Uno GND). Do not connect the motor phases to any Uno pin. Leave the ESC thin red (BEC) wire insulated.
2. Power the ESC/motor from the 12 V supply; the Uno pin is only the signal source.
3. Upload `04_esc_fan_ramp_test`. Open the Serial Monitor at **115200 baud**, line ending **Newline**.
4. Wait for the 3-second arming delay (arming beeps), then send one speed number per line, such as `65`.

## Safety and limits

- The 1350 µs setting caps the ESC command relative to the assumed 1000–2000 µs range. Verify the ESC's actual input range and calibration before connecting the motor.
- A 35% command cap does **not** guarantee current will stay below 5 A. Current depends on load, propeller/fan, motor, ESC, supply behavior, and startup/stall conditions. Use appropriate current monitoring and a supply/ESC/motor combination that can tolerate the load.
- Keep the propeller/fan removed or the motor mechanically secured during initial signal checks. Ensure the ESC enters its expected arming state and that 0 input produces the minimum pulse.
- The speed percentage is a requested throttle command, not measured RPM. No RPM feedback or closed-loop speed regulation is implemented.
- The Servo library uses Timer1, which disables `analogWrite` (PWM) on Uno pins 9 and 10. Nothing in this project uses them for that.
