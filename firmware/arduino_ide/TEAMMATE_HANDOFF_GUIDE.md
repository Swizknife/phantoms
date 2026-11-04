# Follow-Me Fan — Teammate Build & Test Guide (Arduino IDE, Arduino Uno)

This guide assumes you have **never used this project before**. Follow it in
order, top to bottom. Do not skip the safety checklist in each step. The full
wiring and bench procedure is also in `../../ARDUINO_UNO_BUILD_GUIDE.md`.

**What this project is:** an Arduino Uno spins a thermal sensor
around to find people, then swings a fan to point at them and speeds it up or
down depending on how warm they are. We test every part separately before
connecting them all together, so that if something doesn't work, we know
exactly which part is the problem.

---

## Part A — One-time computer setup

### A1. Install Arduino IDE
1. Download the Arduino IDE **desktop app** (2.x) from arduino.cc and install it.
2. Open it once so it finishes its first-time setup.

### A2. Install the one library you need
The Uno is supported out of the box — no board package to install.
1. **Tools → Manage Libraries**, search `Servo`, install **Servo by Arduino**.

`Wire` and `EEPROM` come bundled with the IDE.

### A3. Connect the board and select it
1. Plug the Arduino Uno into your PC with a USB-A to USB-B cable.
2. **Tools → Board → Arduino AVR Boards → Arduino Uno**.
3. **Tools → Port** → pick the new COM port that appeared when you plugged in.
   If no port appears, you probably have a clone with a **CH340** USB chip —
   install the CH340 driver and replug.
4. Click the checkmark (Verify) on any sketch to confirm the board compiles
   before you move on. Close the Serial Monitor before every upload.

### A4. How to use the Serial Monitor (used in every test below)
- **Tools → Serial Monitor** (or the magnifying-glass icon, top right).
- Set the baud rate dropdown (bottom right of the Serial Monitor window) to
  **115200**.
- Set the line ending dropdown to **Newline** (or "Both NL & CR" also works).
- Type into the box at the top and press Enter / click Send to send a value
  to the board.
- Opening the Serial Monitor **restarts the Uno**. That is normal.

---

## Part B — Safety rules (read before every single test)

- [ ] **Fan blades stay off the motor** for every test except the very last one
      in Part D.
- [ ] 12V supply only ever connects to: the LM2596 input, the ESC's thick
      red/black wires, and the Uno's **VIN** pin (there is no tilt motor or
      motor driver). **Never**
      to the 5V pin, the 3.3V pin, or the MLX90614 sensor.
- [ ] The LM2596 must already be trimmed to 5.00–5.10V (measured with a
      multimeter, nothing else connected) before anything else is powered —
      ask whoever did this if you're not sure it's done.
- [ ] The ESC's thin **red** wire (middle of its 3-wire signal plug) must be
      cut or taped off — never connected to anything.
- [ ] The servo is powered from the LM2596 only, **never** from the Uno's 5V pin.
- [ ] Keep one hand near the power switch / USB cable during every motor test
      so you can cut power immediately if anything sounds or smells wrong.

---

## Part C — Component-by-component tests

Do these **in order**. Each one is its own sketch folder under `arduino_ide/`.
Open a sketch by double-clicking its `.ino` file — Arduino IDE will open the
whole folder as that sketch.

### C1. `01_board_test` — does the IDE talk to the board at all?
- **Wiring needed:** none, just USB.
- Open `01_board_test/01_board_test.ino`, click Upload (the arrow icon).
- Open Serial Monitor (115200 baud).
- ✅ **Pass:** the on-board LED (marked "L") blinks, and you see
  `uptime=... freeRAM=...B cpu=16MHz` once a second.
- ❌ If upload fails: re-check Part A3 (board/port/CH340 driver/cable).

### C2. `02_mlx90614_sensor_test` — thermal sensor + wiring
- **Wiring:** MLX90614 VIN→**3.3V**, GND→GND, SDA→**A4**, SCL→**A5**, plus the
  two 4.7kΩ resistors (SDA→3.3V and SCL→3.3V — **not 5V**). See
  `COMPLETE_COMPONENT_CONNECTIONS_REFERENCE.md` in the parent folder for wire
  colors.
- Upload `02_mlx90614_sensor_test.ino`. Open Serial Monitor.
- ✅ **Pass:** `device ACKs at 0x5A`, then `Ambient read: OK` and lines like
  `-> Ambient = 26.40 C` / `-> Object = 27.10 C` once per second, with believable
  numbers. Object temperature should rise if you point it at your hand or face.
- ❌ If it says `NOTHING acks at 0x5A`, read the `Bus idle levels` lines above it:
  - A line **LOW** → pull-up not reaching 3.3V, short to GND, or sensor unpowered.
  - Both **HIGH** but still nothing → check power **at the sensor's own pins**
    with a multimeter (about 3.3V), swap SDA/SCL, check each jumper for
    continuity, and wiggle breadboard contacts. A GY-906 module can also be
    powered from 5V on its VIN (it has its own regulator); a bare chip cannot.

### C3. `03_servo_continuous_test` — yaw servo
- **Wiring:** Servo signal (orange)→**D9**. Servo power (red)→LM2596 OUT+
  (5.05V) — **not** from the Uno. Servo ground (brown)→star ground (which also
  connects to an Uno GND pin). Solder the 1000µF capacitor across the servo's
  red/brown wires.
- Upload the sketch. Open Serial Monitor, type `1500`, press Enter.
- Try nearby numbers (1480, 1520, 1540...) until the shaft is **completely
  still**. Write down that number — this is the "stop pulse" you'll need
  later for calibration.
- Then try a number about 60 below your stop value, confirm it spins one way;
  about 60 above, confirm it spins the other way.
- ✅ **Pass:** shaft spins continuously (doesn't stop at some fixed angle) on
  either side of the stop value you found.
- **Direction:** the head must sweep **clockwise** (seen from above) first.
  If "stop − 60" turns it **anticlockwise**, open `follow_me_fan_full/config.h`
  and set `YAW_REVERSE = true`.
- ❌ If the shaft instead **moves to an angle and holds** no matter what you
  type: **stop here and tell the team** — this means it's a regular
  (non-continuous) servo, and the final firmware needs to change before you
  can continue to Part D.

### C4. `04_esc_fan_ramp_test` — ESC + motor (⚠️ blades off)
- **Wiring:** ESC thin white→**D10**, thin black→star ground, thick red→12V(+),
  thick black→star ground, motor's 3 wires→ESC's 3 blue wires (any order for
  now). ESC's thin **red** wire stays disconnected/taped.
- **Double-check the fan blades are off the motor before powering on.**
- Upload the sketch, power the 12V supply on. You should hear the ESC's
  arming beeps over about 3 seconds.
- In Serial Monitor, type `30`, press Enter — motor should spin up gently
  over ~1 second. Try `0`, `60`, `100` too.
- ✅ **Pass:** smooth ramps up and down, no harsh jumps, motor spins freely.
- ❌ If nothing happens: recheck the white signal wire is on D10 and that
  the thin black wire shares the same ground as the Uno.

### C5. `05_pir_test` — HC-SR501 PIR (only if the MLX90614 doesn't work)
- **Wiring:** PIR VCC→Uno **5V**, OUT→**D2**, GND→Uno GND. Jumper on **H**,
  TIME pot fully anticlockwise, SENS pot in the middle. USB power is enough.
- Upload, open Serial Monitor (115200). Wait out the 60 s warm-up.
- ✅ **Pass:** waving a hand prints `MOTION`; about 3 s after you stop it
  prints `clear`. The Uno's "L" LED follows the sensor.
- The full firmware switches to the PIR **by itself** when no MLX90614
  answers at boot. Mount the PIR on the head where the MLX90614 was.

### C6. `06_head_sweep_test` — full head sweep
- **Wiring:** same as C3, with the fan head (fan + sensor) mounted on the
  servo and the cables routed with a 20–25 cm slack loop.
- At the top of the sketch, set `STOP_US` (from C3) and `YAW_REVERSE` (from
  C3). Leave `OMEGA_FWD`/`OMEGA_BACK` at 30 for the first try.
- Mark the head's start position. Upload and type `g`.
- ✅ **Pass:** the head turns **clockwise** about one full turn, pauses, then
  turns **anticlockwise** back to about the start mark. Cables never pull tight.
- If it under- or over-shoots, scale `OMEGA_*` (the comment in the sketch shows
  how). The full firmware's `cal` command measures these speeds for you.

---

## Part D — Full integration (only after the tests above pass)

1. Open `arduino_ide/follow_me_fan_full/follow_me_fan_full.ino`. Arduino IDE
   will show several tabs along the top (`config.h`, `yaw.cpp`, `thermal.cpp`,
   etc.) — that's normal, it's all one sketch split into files. Don't rename
   or move any of these files out of the folder.
2. Wire up **everything** at once now, following
   `COMPLETE_COMPONENT_CONNECTIONS_REFERENCE.md` exactly. Re-walk the Part B
   safety checklist.
3. **Fan blades still off** for this first run.
4. Upload. Open Serial Monitor at 115200 baud. You'll see:
   ```
   === Follow-Me Fan firmware (Arduino Uno) ===
   Servo not calibrated yet. Run 'cal' before autonomous operation.
   Arming ESC (blades off!)...
   Ready. Commands: status | cal | scan | profile | log on|off | mode 0-3 | stop
   ```
5. Type `cal` and press Enter, then follow its on-screen prompts — this is
   the same stop-pulse search you already practiced in C3, plus two timed
   spin tests where you tell it how many degrees the head turned. Afterwards
   point the head at its start position and type `scan`.
6. Watch the scan sweep, and check the printed result against where
   people actually are standing.
7. Type `mode 1`, `mode 2`, `mode 3` to check each fan speed, then `mode 0`
   to hand control back to automatic.
8. Only once all of that behaves correctly: power off, attach the fan blades,
   and power back on for the first full real-world run.

Full detail on every one of these commands, how rescanning works, and how to
tune things afterward is in `../README.md` (same folder as the main firmware,
one level up) — read that once you're comfortable with the basics above.

**If you get stuck at any step, stop and report back which test number and
what you saw, rather than guessing and continuing.**
