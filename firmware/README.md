# Follow-Me Fan — Implementation & Execution Guide (Arduino Uno)

This firmware runs on an **Arduino Uno (ATmega328P)**. The full wiring and a step-by-step bench
guide are in [`../ARDUINO_UNO_BUILD_GUIDE.md`](../ARDUINO_UNO_BUILD_GUIDE.md). It does two things:

1. Scans with the MLX90614 while spinning the fan head, finds where people are,
   and swings the head only across that arc.
2. Picks one of three fan speeds from how much hotter the person is than the room.

All sources live in `arduino_ide/follow_me_fan_full/` (one copy only — the Arduino IDE and
PlatformIO both build from it). This file is about building, flashing, calibrating, and running it
safely.

---

## 1. Before you power anything on

- [ ] Fan blades **removed** from the BLDC motor.
- [ ] LM2596 output measured and trimmed to **5.00–5.10 V** with a multimeter,
      with nothing else connected to it yet (see the hardware handoff's Step 1).
- [ ] SimonK ESC's thin **red BEC wire** is cut/taped off — not connected to anything.
- [ ] All grounds (adapter, LM2596, Uno, servo, ESC, MLX90614, PIR if present) meet at the
      one star-ground point.
- [ ] MLX90614 `VIN` goes to the Uno **3.3V** pin, never 5 V or 12 V.
- [ ] The two 4.7 kΩ pull-ups go to **3.3V**, not 5 V (the firmware switches the Uno's internal
      5 V pull-ups off).
- [ ] You know this is a **TowerPro continuous-rotation ("360°") servo** — the first
      test below (§5) confirms it. If it turns out to hold a fixed angle instead,
      stop and tell me; the yaw code assumes continuous rotation.

---

## 2. Install the toolchain

**Arduino IDE (recommended):**
1. Install Arduino IDE 2.x. The Uno needs no extra board package.
2. **Tools → Manage Libraries → search `Servo` → Install** (by Arduino). `Wire` and `EEPROM`
   ship with the IDE.
3. Clone/Chinese Uno boards use a **CH340** USB chip: install the CH340 driver if no COM port
   appears.

**PlatformIO (optional):** open `firmware/` in VS Code. `platformio.ini` has one environment,
`uno`, and builds the same `arduino_ide/follow_me_fan_full/` sources.

---

## 3. Build and flash

**Arduino IDE:** open `arduino_ide/follow_me_fan_full/follow_me_fan_full.ino`, **Tools → Board →
Arduino AVR Boards → Arduino Uno**, pick the COM port, click Upload. Close the Serial Monitor
before uploading.

**PlatformIO:** `pio run -t upload -t monitor`

The Serial Monitor must be at **115200 baud**, line ending **Newline**. Opening the Serial Monitor
**resets the Uno** (normal for this board), so the boot messages always appear:

```
=== Follow-Me Fan firmware (Arduino Uno) ===
Servo not calibrated yet. Run 'cal' before autonomous operation.
Arming ESC (blades off!)...
Ready. Commands: status | cal | scan | profile | log on|off | mode 0-3 | stop
```

If instead you see `FATAL: MLX90614 not found at 0x5A`, stop and check the I²C
wiring (SDA=A4, SCL=A5, two 4.7kΩ pull-ups to 3.3V) before continuing. Test 2 in
`arduino_ide/02_mlx90614_sensor_test/` prints a full diagnosis.

The ESC arms for 3 seconds at boot (`ESC_ARM_MS` in `config.h`) — you should hear
the SimonK's arming beeps. **Blades must still be off at this point.**

Right after arming, the firmware **immediately starts a full scan** — the servo
will start moving. With no calibration yet, the angle estimate is meaningless, so
do §5 next before anything else.

---

## 4. Serial commands reference

Type these into the serial monitor (115200 baud, Newline line-ending) at any time:

| Command | What it does |
|---|---|
| `status` | Prints state, angle, fan output, mode, ambient temp, swing arc, calibration, free RAM |
| `cal` | Interactive servo calibration (§5) — **run this first**. Leaves the system halted; type `scan` afterwards. |
| `scan` | Forces an immediate full 360° scan (home → sweep forward → sweep back). Also resumes after `stop`. |
| `profile` | Prints the stored 36-bin temperature profile (forward/back averages) as CSV |
| `log on` / `log off` | Streams `angle,tobj,ta` CSV for every sensor sample, and prints each warm group after a full scan |
| `mode 0` | Back to automatic fan speed |
| `mode 1` / `mode 2` / `mode 3` | Force a fan speed mode manually (applies immediately, overrides auto) |
| `stop` | Fan to minimum pulse, servo halted, **stays halted** until you type `scan` |

Keep a finger near `stop` (or just unplug 12V) during every test below.

---

## 5. Step 1 — Confirm the servo and calibrate (`cal`)

This is mandatory before any autonomous run; the yaw code has no position
feedback, so it only knows the angle because it integrates (speed × time), and it
needs that speed measured first.

1. Point the fan head at a reference mark (e.g. tape a line on the base and the
   head) so you can read degrees turned by eye.
2. Type `cal` and press Enter.
3. **Stop-pulse trim:** it will repeatedly show `trial stop = 1500 us` (adjustable).
   Type `u` / `d` to nudge it ±5µs, and watch the shaft:
   - If the shaft **keeps spinning** no matter what you send near 1500µs until a
     wide range, it's likely a positional (180°) servo after all — **stop and
     tell me**, don't continue.
   - If it's a true continuous-rotation servo, there's a pulse width (near 1500µs,
     maybe off by 20–80µs due to trim) where it sits still. Nudge until it does,
     then type `s`.
4. It then spins **forward** for 10 seconds at a fixed offset from the stop pulse.
   Forward must be **clockwise** seen from above. If it turns anticlockwise, set
   `YAW_REVERSE = true` in `config.h`, re-upload and run `cal` again.
   Watch the mark and count degrees turned (estimate to the nearest ~5°, e.g. "it
   went around once and a quarter" = 450°). When it stops moving, type that
   number and Enter.
5. Same for **backward** — type the degrees turned.
6. It prints and saves `stopUs`, `wFwd`, `wBack` (deg/s) to the Uno's **EEPROM**. These persist
   across reboots and re-uploads — you only need to redo `cal` if you change the servo, its
   wiring, or its supply voltage.
7. Point the head at its start position (angle 0) and type `scan`.

Sanity check with `status`: `wFwd`/`wBack` should be in the same ballpark (big
asymmetry, e.g. 40 vs 10 deg/s, means something is mechanically binding one way).

If `wFwd` or `wBack` ends up too low (<1 deg/s) the firmware refuses to save and
tells you to check wiring — redo `cal`.

---

## 6. Step 2 — Scan test with blades off (`scan` + `log on`)

1. Type `log on`, then `scan`.
2. Watch the servo do: home to 0° → sweep to 360° → sweep back to 0°. This takes
   roughly 360°/30°·s ×2 ≈ 24 seconds at the default scan speed.
3. Try it with **nobody in front of the sensor**, then **one person standing** at
   a few different angles/distances, then **two people** at once. For each run,
   check the `[scan] N people, arc=[L,R] delta=X.XX` line against where you
   actually stood. With `log on`, each warm group is also listed as `[group] edges=… delta=…`.
4. Use `log off` to stop the CSV stream once you're done capturing a run — it's
   verbose.

What to tune in `arduino_ide/follow_me_fan_full/config.h` if detection looks wrong:
- Readings never rise much above baseline → lower `RISE_THRESH_C` or move the
  sensor closer / check it's aimed correctly.
- A person isn't flagged even though the bump is visible in the log → lower
  `PERSON_DELTA_C`.
- The detected arc is much wider/narrower than the person actually is → adjust
  `FOV_EFF_DEG` (raise it if the arc is too wide, lower it if too narrow) to match
  your MLX90614's real field of view.
- Two people close together merge into one block → that's expected below ~90°
  separation; the swing will still cover both, which is intended behavior.

---

## 7. Step 3 — Fan speed test, still with blades off (`mode 1/2/3`, `stop`)

1. `mode 1`, then `mode 2`, then `mode 3` — each should ramp the ESC pulse
   smoothly (watch `status`'s `fanUs` climb/fall at ~140µs/s) between 1150, 1250,
   1350µs. Listen/feel for three audibly different speeds.
2. `mode 0` returns to automatic control (driven by the Δ from the last scan).
3. `stop` at any time cuts the fan back to the 1000µs minimum pulse and halts the
   servo until you type `scan`.

Do **not** attach blades until this step behaves exactly as expected.

---

## 8. Step 4 — Full run with blades on

1. Re-check every box in §1.
2. Power on, let it arm, let the initial scan run.
3. Confirm with `status` that the arc and mode track a person walking around in
   front of it, and that current draw stays sane (use a multimeter/clamp on the
   12V line — should stay under 5A; if it trips, drop back to `mode 1`/`2` only
   until you've characterized the motor's real draw).
4. Walk out of view and confirm: after `LOST_PASSES` (2) empty tracking passes,
   it rescans; if the rescan also finds nobody, the fan ramps to minimum and it
   waits `IDLE_RESCAN_MS` (10s) between rescans.
5. Let it run ~10 minutes and check how far the physical head has drifted from
   where the open-loop angle estimate thinks it is (compare against your tape
   mark). Some drift is expected — this is the known limitation of open-loop
   continuous-servo control. If it's severe, re-run `cal`, and consider lowering
   `SERVO_SCAN_OFFSET_US` in `config.h` for a slower, more accurate scan.

---

## 9. How scanning, tracking and rescanning work

1. **Full scan (about 25 s):** the head homes to 0°, sweeps to 360° and back. Readings are stored
   in 36 bins of 10°, forward and back separately, then averaged so sensor lag cancels.
2. **Analysis:** the median of all bins is the room baseline. Runs of bins at least
   `RISE_THRESH_C` above it are warm groups; a group whose peak is at least `PERSON_DELTA_C`
   above ambient is a person. The swing arc covers all people, with edges pulled in by half the
   sensor FOV and widened by `SWING_MARGIN_DEG`.
3. **Tracking:** the head swings back and forth across the arc. Each swing refreshes only that
   arc's bins in one direction, then re-analyzes, so the arc and the fan speed follow the person.
4. **A full rescan happens:**
   - every `TRACK_RESCAN_MS` (**60 s**) while tracking,
   - after `LOST_PASSES` (**2**) swings in a row with no person,
   - every `IDLE_RESCAN_MS` (**10 s**) while nobody is found,
   - when the sensor comes back after a fault, or when you type `scan`.
5. **Fan speed** follows the hottest person's delta: mode 2 at ≥2.0 °C, mode 3 at ≥4.0 °C, falling
   back at 3.5 °C / 1.5 °C. A detected person always gets at least mode 1. Mode changes wait
   `MODE_MIN_DWELL_MS` (10 s) so the fan doesn't hunt.
6. **Direction:** the forward sweep (0→360°) is clockwise and the return is anticlockwise, set by
   `YAW_REVERSE` in `config.h`.

### PIR backup mode (automatic)

If no MLX90614 answers at boot, the firmware switches to the HC-SR501 PIR on **D2** and prints
`MLX90614 not found at 0x5A -> using the HC-SR501 PIR on D2 instead`. `status` then shows
`sensor=PIR`. Mount the PIR on the head in place of the MLX90614, and test it first with
`arduino_ide/05_pir_test`.

- **Warm-up:** it waits `PIR_WARMUP_MS` (60 s) after power-on before the first scan.
- **Stepped scan:** the head turns clockwise in `PIR_STEP_DEG` (30°) steps. At each stop it waits
  for OUT to drop (`PIR_SETTLE_*`), then watches for `PIR_WATCH_MS` (2.5 s). A full scan takes
  about 1–2 minutes.
- **Tracking:** it swings over the arc where motion was seen, looking again at each end. After
  `PIR_LOST_PASSES` (6) empty looks it rescans, and it also rescans every `PIR_TRACK_RESCAN_MS` (3 min).
- **Fan:** a fixed `PIR_FAN_MODE` (2) while someone is detected. `mode 1-3` still overrides it.
- **Limitation:** a PIR sees *movement*, not heat, so someone sitting very still may be lost and
  picked up again on the next rescan.

---

## 10. Where to tune later

Everything adjustable lives in one file: **`arduino_ide/follow_me_fan_full/config.h`**. After
flashing a changed value, just re-upload — calibration is stored separately in EEPROM and is kept.

Most likely things you'll revisit after bench testing:
- `RISE_THRESH_C`, `PERSON_DELTA_C`, `FOV_EFF_DEG` — detection sensitivity (§6).
- `MODE_UP_2/3`, `MODE_DOWN_1/2` — speed-mode thresholds, once you've logged real
  Δ values for "close", "medium", "far" people with `log on`.
- `MODE_PULSE_US` / `ESC_MAX_US` — fan speed/current ceiling, once you've measured
  real current draw with blades on.
- `TRACK_RESCAN_MS`, `IDLE_RESCAN_MS`, `LOST_PASSES` — how often it re-looks (§9).
- `OMEGA_DEFAULT_DEG_S` / `SERVO_SCAN_OFFSET_US` — scan speed vs. accuracy
  trade-off (this gets overwritten by `cal`'s measured value at runtime anyway).
- `YAW_REVERSE` — flip it if the first sweep turns anticlockwise.
- `PIR_*` — PIR backup timing and fan mode (only used when no MLX90614 is found).

To erase saved calibration and start over, upload any sketch that writes `0xFF` to EEPROM
address 0–7 (the Arduino IDE example **File → Examples → EEPROM → eeprom_clear** does this).

---

## 11. Changelog: ESP32-S3 → Arduino Uno

**Hardware:** ESP32-S3-DevKitC-1 replaced by an Arduino Uno. Pins are now SDA=A4, SCL=A5,
servo=D9, ESC=D10; the tilt (L298N on D7/D8) has been removed (full table in `../ARDUINO_UNO_BUILD_GUIDE.md`). The Uno
is powered from the 12 V adapter on its VIN pin (or USB for bench work); the 3.3 V sensor uses
the Uno's 3.3V pin.

**Porting (no behaviour change):** LEDC PWM → `Servo` library; `Preferences` → `EEPROM`;
`Serial.printf` → `Serial.print` with `F()` strings (2 KB RAM); `Wire.begin(sda,scl,hz)` →
`Wire.begin()` + `setClock`; internal I²C pull-ups to 5 V switched off; I²C timeout added so a
stuck bus can't freeze the loop.

**Optimization:** scan state is about 360 bytes (was about 430 plus 3 temporary arrays); no
`String`/heap use; the whole firmware uses about 46 % of RAM and 59 % of flash.

**Bug fixes:**
- *Idle always faulted:* the sensor wasn't read while idle, so ~1 s later the watchdog tripped
  and rescanned immediately. The sensor is now read in every state.
- *Tracking always "lost" the person:* each swing wiped the whole profile, which one arc could
  never refill. Each swing now refreshes only its own arc, one direction.
- *Fan could stay off for a detected person* with a small delta, and manual modes were
  overridden when a scan found nobody. Mode logic rewritten; manual mode is always respected.
- *`stop` didn't stay stopped:* it now halts until you type `scan`.
- *`01_board_test` was a mislabeled copy of the sensor test:* now a real board test.
