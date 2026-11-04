# Arduino Uno Build Guide — Follow-Me Thermal Tracking Fan

One document to build, test and run the project on an **Arduino Uno (ATmega328P)**. It replaces the
ESP32-S3 controller; the approach and goal are unchanged: scan for people with an IR sensor, swing a
fan over them, and set the fan speed from how warm they are.

Deeper references: [`COMPLETE_COMPONENT_CONNECTIONS_REFERENCE.md`](COMPLETE_COMPONENT_CONNECTIONS_REFERENCE.md)
(every wire), [`firmware/README.md`](firmware/README.md) (commands, calibration, tuning).

---

## 1. Parts and what changed

| Part | Notes |
| :--- | :--- |
| Arduino Uno (ATmega328P, 5 V logic) | Replaces the ESP32-S3. Clone boards use a CH340 USB chip (install its driver). |
| MLX90614 (GY-906) | 3.3 V I2C IR sensor, mounted on the fan head. Needs pull-ups **to 3.3V**: use two 4.7 kΩ unless the board already has them. |
| MG996R continuous-rotation (360°) servo | Yaw: turns the head CW 0→360°, then CCW back. Powered from the LM2596, never from the Uno. |
| SimonK 30 A ESC + A2212 1000 KV BLDC fan | ESC red (BEC) wire insulated. |
| LM2596 buck converter | Trimmed to 5.05 V, feeds the **servo only**. |
| 12 V 5 A adapter (alligator clips) | Feeds LM2596, ESC, Uno `VIN` through a screw-terminal bus. A USB power bank can't run the motors. |
| 1000 µF cap, 2 × 4.7 kΩ | Servo decoupling, I2C pull-ups. |
| HC-SR501 PIR (backup) | Only if the MLX90614 fails: `VCC`→5V, `OUT`→D2, `GND`→GND, mounted on the head. The firmware switches to it automatically. |

There is no tilt mechanism and no motor driver (the L298N was removed). The only motors are the servo and the BLDC.

**What changed from the ESP32-S3 build:** only the controller. The Uno is a 5 V, 2 KB-RAM board,
so the firmware uses the `Servo` library instead of LEDC PWM, EEPROM instead of NVS, and the
I2C pull-ups must go to 3.3V (the Uno's internal 5 V pull-ups are switched off in code).

---

## 2. Pin connection summary

| Signal | Uno pin | Other end |
| :--- | :--- | :--- |
| Sensor power | **3.3V** | MLX90614 `VIN` + one leg of each 4.7 kΩ |
| Sensor ground | **GND** | MLX90614 `GND` |
| I2C data | **A4 (SDA)** | MLX90614 `SDA` + other leg of resistor 1 |
| I2C clock | **A5 (SCL)** | MLX90614 `SCL` + other leg of resistor 2 |
| Servo signal | **D9** | MG996R orange |
| ESC signal | **D10** | SimonK white |
| PIR output (backup only) | **D2** | HC-SR501 `OUT` |
| PIR power (backup only) | **5V** | HC-SR501 `VCC` |
| Uno power | **VIN** | 12 V adapter (+) (or USB for bench work) |
| Ground | **GND** | Star ground |
| Not connected | `D7`, `D8`, `D0`, `D1` (and `5V` when there's no PIR) | — |

Other power wiring (unchanged): LM2596 `IN+/IN-` ← adapter; LM2596 `OUT+` → servo red and the
1000 µF cap; ESC thick red/black ← adapter; ESC thin black → star ground; servo brown and cap `−`
→ star ground. **Never** connect the LM2596 output to the Uno `5V` pin.

```
                          ARDUINO UNO  (top view, USB at the bottom-left)

   DIGITAL HEADER                                              POWER HEADER
   ┌──────────────────────────────────────────┐   ┌──────────────────────────────────────┐
   │ D13 D12 D11 [D10] [D9] D8 D7 ... [D2] D1 D0  │   │ IOREF RESET [3.3V] 5V GND [GND] [VIN] │
   └────────────────┬─────┬─────────────┬─────────┘   └───────────┬────────────────┬──────┬───┘
                    │     │             │                          │                │      │
              ESC white   │         PIR OUT                   MLX VIN + 2×4.7k    Star    12V adapter
              (SimonK)    │         (backup)                  (pull-ups to 3.3V)  GND     (+)
                     Servo orange                                                       (5V pin: PIR VCC only)
                     (MG996R)
   ANALOG HEADER
   ┌────────────────────────────────────────────┐
   │ A0  A1  A2  A3  [A4 = SDA]  [A5 = SCL]       │
   └──────────────────────┬──────────┬──────────┘
                          │          │
                   MLX90614 SDA   MLX90614 SCL
                   + 4.7k → 3.3V  + 4.7k → 3.3V

 12V adapter (+) ──┬── LM2596 IN+ ──► OUT+ (5.05V) ──► Servo red + 1000µF (+)   (servo only)
                   ├── SimonK thick red
                   └── Uno VIN
 All grounds (adapter −, LM2596, Uno GND, ESC black ×2, servo brown, cap −, MLX GND, PIR GND)
 meet at ONE star-ground point.
```

---

## 3. Before first power-up (multimeter)

1. Adapter + LM2596 only: trim `OUT+` to **5.05 V** (5.00–5.10 V). Power off.
2. Uno on USB only: measure **3.3V pin ≈ 3.3 V**, then connect the sensor.
3. Confirm ESC thin red wire is cut/taped, and fan blades are **off**.
4. Confirm no 12 V wire touches the Uno except `VIN`.

---

## 4. Arduino IDE setup

1. Install Arduino IDE 2.x. **Tools → Board → Arduino AVR Boards → Arduino Uno**; pick the COM port.
2. **Tools → Manage Libraries → install `Servo`** (by Arduino). `Wire` and `EEPROM` are built in.
3. Serial Monitor: **115200 baud**, line ending **Newline**. Opening it resets the Uno (normal).
4. Close the Serial Monitor before each upload. If a sketch was already open, close its tab and
   reopen the `.ino` from disk so the IDE doesn't overwrite it with an old copy.

---

## 5. Test sequence (do them in order)

| # | Sketch (`firmware/arduino_ide/`) | Wiring needed | Pass looks like |
| :-: | :--- | :--- | :--- |
| 1 | `01_board_test` | USB only | LED blinks; `uptime=… freeRAM=…B cpu=16MHz` each second |
| 2 | `02_mlx90614_sensor_test` | sensor (+ 2 pull-ups if the GY-906 has none) | `device ACKs at 0x5A`, `Ambient read: OK`, room-temperature numbers that rise when you point at a hand |
| 3 | `03_servo_continuous_test` | servo + LM2596 + cap | Type `1500`…; find the pulse where the shaft is still; ±60 µs spins each way. Note which way stop−60 turns: clockwise → `YAW_REVERSE = false`, anticlockwise → `true` (in `config.h`) |
| 4 | `04_esc_fan_ramp_test` (**blades off**) | ESC + A2212 + 12 V | Arming beeps; `30`, `60`, `100` ramp smoothly; `0` stops |
| 5 | `05_pir_test` (only if using the PIR backup) | HC-SR501: VCC→5V, OUT→D2, GND | 60 s warm-up, then `MOTION` when you wave, `clear` ~3 s later |
| 6 | `06_head_sweep_test` | servo + mounted head + 12 V | `g`: head turns CW one full turn, pauses, comes back CCW to the start mark; cables never pull tight |

**PIR backup:** the main firmware uses the PIR **automatically** when no MLX90614 answers at boot
(`status` shows `sensor=PIR`). The PIR goes on the rotating head in place of the MLX90614. The head
then scans in 30° steps, stopping about 3–8 s at each step for a reading, and runs the fan at mode 2
when it sees someone.

**If test 2 says `NOTHING acks at 0x5A`:** read the `Bus idle levels` lines.
- A line **LOW** → pull-up not on 3.3V, a short to ground, or the sensor is unpowered.
- Both **HIGH** → bus is fine, so the sensor isn't answering. Measure VIN–GND *at the sensor's pins*
  (≈ 3.3 V), check continuity from A4/A5 to the sensor's SDA/SCL (and that they're not swapped),
  wiggle breadboard contacts, and try another sensor (a part once hit with 5 V/12 V can be dead).
  A GY-906 module may be powered from 5 V `VIN` (it has a regulator); a bare chip may not.

**Servo not continuous?** If the shaft moves to an angle and holds, stop: the yaw code needs a
continuous-rotation servo.

---

## 6. Full firmware

1. Open `firmware/arduino_ide/follow_me_fan_full/follow_me_fan_full.ino` (the tabs are one sketch),
   wire everything, blades still **off**, upload.
2. Boot output: `=== Follow-Me Fan firmware (Arduino Uno) ===` … `Ready. Commands: …`
3. `cal` → finds the stop pulse, then measures deg/s each way and saves to EEPROM. Point the head at
   its start position (angle 0), then type `scan`.
4. `log on` + `scan`: check `[scan] N people, arc=[L,R] delta=X` against where people stood.
5. `mode 1`, `mode 2`, `mode 3`: check fan speeds (`status` shows `fanUs`). `mode 0` = automatic.
6. Only then fit the blades.

| Command | Effect |
| :--- | :--- |
| `status` | state, angle, fan, mode, ambient, arc, calibration, free RAM |
| `cal` | servo calibration (leaves system halted; then `scan`) |
| `scan` | full 360° scan now; also resumes after `stop` |
| `profile` | print the 36-bin temperature profile |
| `log on` / `log off` | stream `angle,tobj,ta`; list warm groups after scans |
| `mode 0-3` | 0 = auto, 1–3 = manual speed |
| `stop` | fan off, servo halted, stays halted until `scan` |

---

## 7. How scanning, tracking and rescanning work

- **Full scan (~25 s):** head homes to 0°, sweeps to 360° and back, logging temperature in 36 bins of
  10°. Forward and back readings are averaged so sensor lag cancels.
- **Finding people:** a bin run at least `RISE_THRESH_C` (0.8 °C) above the room median is a warm
  group; if its peak is `PERSON_DELTA_C` (1.0 °C) above ambient it is a person. One swing arc covers
  all people (edges pulled in by half the sensor FOV, plus a 15° margin).
- **Tracking:** the head swings back and forth across that arc continuously. Each swing refreshes
  the arc's bins and re-checks, so the arc and fan speed follow the person.
- **Full rescan happens:**

  | Trigger | Value |
  | :--- | :--- |
  | While tracking, periodically | every **60 s** (`TRACK_RESCAN_MS`) |
  | Person lost | after **2** empty swings in a row (`LOST_PASSES`) |
  | Nobody found | every **10 s** (`IDLE_RESCAN_MS`) |
  | Sensor recovers from a fault, or `scan` typed | immediately |

- **Fan speed:** delta = hottest person − ambient. Mode 2 at ≥ 2.0 °C, mode 3 at ≥ 4.0 °C, drops back
  at 3.5 °C / 1.5 °C; any person ≥ mode 1; changes wait 10 s (`MODE_MIN_DWELL_MS`).
- **Safety:** if the sensor gives no valid reading for 1 s, the fan and servo stop until it recovers.

All values above are in `firmware/arduino_ide/follow_me_fan_full/config.h`.

---

## 8. Uno-specific notes

- **RAM is 2 KB.** The firmware uses about 46 % (954 B) and keeps ~1.1 KB free for the stack. Don't
  add `String`, `printf` or large arrays; wrap new text in `F("…")`.
- **Open-loop yaw:** the angle is estimated from time × calibrated speed, so it drifts slowly.
  Re-run `cal` after changing the servo, its wiring, or its supply voltage.
- **Servo library uses Timer1:** `analogWrite` on D9/D10 won't work (not used here).
- **3.3 V logic level:** the sensor's 3.3 V high is above the Uno's input threshold (≈ 3.0 V), so
  I2C works with the pull-ups on 3.3V. Keep wires short.
