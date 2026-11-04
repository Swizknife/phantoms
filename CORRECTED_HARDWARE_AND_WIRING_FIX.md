# Corrected Hardware Repair & Implementation Manual
## Step-by-Step Fix Guide for the Arduino Uno Thermal-Tracking "Follow-Me" Fan

> The controller was changed from an ESP32-S3-DevKitC-1 to an **Arduino Uno**. Everything else
> (sensor, servo, ESC, power tree, behaviour) is unchanged. The complete bench procedure is in
> [`ARDUINO_UNO_BUILD_GUIDE.md`](ARDUINO_UNO_BUILD_GUIDE.md).
>
> **No tilt, no motor driver:** the L298N and DC tilt motor have been removed. The only motors are
> the MG996R 360° servo and the A2212 BLDC (via the SimonK ESC). An HC-SR501 PIR on D2 is the
> backup sensor. Full wire list:
> [`COMPLETE_COMPONENT_CONNECTIONS_REFERENCE.md`](COMPLETE_COMPONENT_CONNECTIONS_REFERENCE.md).

---

## 1. Quick Summary of What to Change

If you previously followed the flawed conceptual diagrams, make these **7 mandatory physical fixes** before powering on:

```
+------------------------------------------------------------------------------------------------------+
|                                     MANDATORY HARDWARE FIXES                                         |
+------------------------------------------------------------------------------------------------------+
|  [OLD FLAWED DESIGN]                                       [NEW VERIFIED FIX]                        |
|                                                                                                      |
|  1. 12V adapter rail connected to 3.3V bus ----------> Solder I2C pull-ups & sensor to Uno 3.3V pin  |
|  2. 3.3V fed into the controller's power pin --------> Uno VIN <- 12V adapter (or USB); LM2596 5.05V |
|                                                         feeds the SERVO only, never the Uno 5V pin   |
|  3. ESC phase U/V wires connected to controller pins -> Disconnect; U, V, W go ONLY to BLDC motor     |
|  4. Controller TX pin labeled "12V Power" to ESC ----> Power ESC with thick 12V/GND leads from adapter|
|  5. ESC labeled as I2C (SDA/SCL) --------------------> Drive ESC using 1-channel pulse on Uno D10     |
|  6. 6N137 optocoupler unpowered / floating ----------> Remove 6N137; wire D10 directly to ESC signal  |
|  7. L298N + DC tilt motor on D7/D8 -----------------> Removed entirely; D7/D8 stay unconnected      |
+------------------------------------------------------------------------------------------------------+
```

---

## 2. Verified Arduino Uno Pin Reference

```
                              ARDUINO UNO  (pins used by this project)

   DIGITAL HEADER:   D10 -> ESC signal (white)     D9 -> Servo signal (orange)
                     D2  <- HC-SR501 PIR OUT (backup only)
                     D7/D8 = unused (tilt removed)   D0/D1 = USB serial, leave free

   ANALOG HEADER:    A4 = SDA -> MLX90614 SDA (pull-up to 3.3V: GY-906 on-board or 4.7k)
                     A5 = SCL -> MLX90614 SCL (pull-up to 3.3V: GY-906 on-board or 4.7k)

   POWER HEADER:     3.3V -> MLX90614 VIN (+ the two 4.7k pull-ups if fitted)
                     VIN  <- 12V adapter (+)       GND -> Star Ground
                     5V   -> HC-SR501 VCC only (backup), otherwise empty
```

### Direct Wire-by-Wire Interconnect Table

| Peripheral | Terminal / Wire | Connects Directly To | Wire Color / Type | Function |
| :--- | :--- | :--- | :--- | :--- |
| **MLX90614** | `VIN` | Uno **3.3V** | Red (24 AWG) | Clean $3.3\text{V}$ sensor supply |
| **MLX90614** | `GND` | Uno **GND** | Black (24 AWG) | Sensor ground reference |
| **MLX90614** | `SDA` | Uno **A4** | Blue (24 AWG) | I2C Data ($4.7\text{k}\Omega$ to $3.3\text{V}$) |
| **MLX90614** | `SCL` | Uno **A5** | Yellow (24 AWG) | I2C Clock ($4.7\text{k}\Omega$ to $3.3\text{V}$) |
| **MG996R Servo** | `PWM Signal` | Uno **D9** | Orange | 50Hz servo pulse (Servo library) |
| **MG996R Servo** | `VCC (+5V)` | **LM2596 `OUT+`** | Red (20 AWG) | High-current $5\text{V}$ rail |
| **MG996R Servo** | `GND` | **Common Star GND** | Brown / Black | Return ground ($1000\,\mu\text{F}$ cap across VCC/GND)|
| **SimonK 30A ESC**| `PWM Signal` | Uno **D10** | White / Orange | 50Hz throttle pulse (1000-1350 µs) |
| **SimonK 30A ESC**| `Signal GND` | **Common Star GND** | Black (Thin) | Signal ground reference |
| **SimonK 30A ESC**| `BEC (+5V)` | **NO CONNECTION** | Red (Thin) | **Tape off / insulate!** |
| **SimonK 30A ESC**| `+12V In` | **12V Adapter (+)** | Red (Thick 14-16 AWG)| High-current motor supply |
| **SimonK 30A ESC**| `GND In` | **12V Adapter (-)** | Black (Thick 14-16 AWG)| High-current motor ground |
| **SimonK 30A ESC**| `Motor Phases`| **A2212 1000KV BLDC** | Blue (3 wires: U, V, W)| 3-Phase AC stator drive |
| **LM2596 Buck** | `IN+ / IN-` | **12V Adapter (+ / -)** | Red / Black (18 AWG) | Main power input |
| **LM2596 Buck** | `OUT+ (+5.05V)`| **MG996R `Red`** | Red (20 AWG) | Regulated servo power (not the Uno) |
| **LM2596 Buck** | `OUT- (GND)` | **Common Star GND** | Black (18 AWG) | Central ground tie point |
| **Arduino Uno** | `VIN` | **12V Adapter (+)** | Red (20 AWG) | Uno on-board regulator makes its own 5V |
| **Arduino Uno** | `GND` | **Common Star GND** | Black (20 AWG) | Logic ground |
| **HC-SR501 PIR** (backup) | `VCC` | Uno **5V** | Red (24 AWG) | PIR power |
| **HC-SR501 PIR** (backup) | `OUT` | Uno **D2** | Green (24 AWG) | Motion signal (3.3 V high) |
| **HC-SR501 PIR** (backup) | `GND` | **Common Star GND** | Black (24 AWG) | Ground reference |

---

## 3. Corrected Master Circuit Schematic

```
                                 CORRECTED COMPLETE CIRCUIT SCHEMATIC
                                =====================================

      12V 5A DC POWER ADAPTER
     +-----------------------+
     |   (+)           (-)   |
     +----|-------------|----+
          |             |
          +-------------|-------------------------------+
          |             |                               |
          +-------+     |                               |
          |       |     |                               |
          v +12V  v     |                               v (+12V)
     +----------+ +---------+                      +------------+
     |  LM2596  | | ARDUINO |                      | SimonK 30A |
     |   BUCK   | |   UNO   |                      |  BLDC ESC  |
     | STEPDOWN | |  (VIN)  |                      +-----+------+
     +----+-----+ +----+----+                            |
          |            |                                 | 3-Phase AC (U, V, W)
          | +5.05V     | 3.3V pin     5V pin             v
          | (servo     v                |         [A2212 BLDC FAN]
          |  only)  +----------+        v
          v         | MLX90614 |  +-----------+
     +----------+   |  SENSOR  |  | HC-SR501  |
     |  MG996R  |   +----+-----+  | PIR (bkp) |
     |  SERVO   |        |        +-----+-----+
     +----+-----+        |              |
          |              |              |
==========v==============v==============v===========================================
                                   SYSTEM COMMON STAR GROUND (0V)
====================================================================================
```

---

## 4. Corrected Signal & Control Wiring

```
                                ARDUINO UNO
                           +---------------------+
                           |                     |
         [+3.3V Out] ----->| 3.3V                |
                           |                     |
                           |                 D9  |----[50Hz pulse]---------> MG996R Signal (Orange)
                           |                     |
                           |                 D10 |----[50Hz pulse]---------> SimonK ESC Signal (White)
                           |                     |
                           |                 D2  |<---[Motion in]---------- HC-SR501 OUT (backup)
                           |                     |
         [I2C SDA] <---*-->| A4 (SDA)            |
                       |   |                     |
         [I2C SCL] <---*-->| A5 (SCL)            |
                       | | |                     |
                       | | |                 VIN |<---[+12V from adapter]--- 12V Adapter (+)
                       | | |                     |
                       | | |                 GND |<---[Common Star GND]----- System Ground Bus
                       | | +---------------------+
                       | |
                       | +---[ 4.7k Resistor ]---+
                       |                         |
                       +-----[ 4.7k Resistor ]---+
                                                 |
                                                 v
                                        +3.3V Rail (Uno 3.3V pin)
                                                 |
                              +------------------+----------+
                              | MLX90614 INFRARED SENSOR    |
                              |  Pin 1: VIN  <-- +3.3V Rail |
                              |  Pin 2: GND  <-- Common GND |
                              |  Pin 3: SCL  <-- Uno A5     |
                              |  Pin 4: SDA  <-- Uno A4     |
                              +-----------------------------+
```

---

## 5. Step-by-Step Bench Assembly & Modification Procedure

### Step 1: Pre-Power Voltage Tuning (DO NOT SKIP)
1. Do **not** connect the Uno, servo, or sensor yet.
2. Connect your $12\text{V}$ power adapter to the input terminals (`IN+` and `IN-`) of the LM2596 buck converter.
3. Turn on the $12\text{V}$ supply.
4. Place digital multimeter probes on `OUT+` and `OUT-` of the LM2596.
5. Using a small flathead screwdriver, turn the brass potentiometer screw until your multimeter reads **exactly $5.05\text{V} - 5.10\text{V}$**.
6. Turn off the power. The $5\text{V}$ rail is now safe for the MG996R servo.

### Step 2: Preparing the SimonK 30A ESC
1. Locate the 3-wire control lead coming from the ESC (White, Red, Black).
2. The center **Red wire** carries $+5\text{V}$ from the ESC's internal linear BEC. Connecting this red wire in parallel with your LM2596 regulator will cause circulating currents that can overheat both regulators.
3. Gently lift the plastic retaining tab on the servo plug with a hobby knife and slide the Red wire out. Wrap the exposed female pin in heat shrink tubing or electrical tape.
4. Only the **White wire (Signal)** and **Black wire (GND)** will be connected to the system.

### Step 3: Wiring the MLX90614 Sensor
1. Connect sensor `VIN` to Uno **3.3V**.
2. Connect sensor `GND` to Uno **GND**.
3. Connect sensor `SDA` to Uno **A4**.
4. Connect sensor `SCL` to Uno **A5**.
5. Check the GY-906 board for two small resistors next to SDA/SCL (marked `103` or `472`). If they are there, skip steps 6–7.
6. Solder a $4.7\text{k}\Omega$ resistor between **SDA** and **3.3V**.
7. Solder a $4.7\text{k}\Omega$ resistor between **SCL** and **3.3V**.
8. The firmware turns the Uno's internal 5 V pull-ups off, so the 3.3 V pull-ups (on-board or yours) are the only ones on the bus.

### Step 4: Stabilizing the MG996R Servo
1. Connect the servo **Orange wire** to Uno **D9**.
2. Connect the servo **Red wire** to the LM2596 `OUT+` ($+5.05\text{V}$) rail.
3. Connect the servo **Brown wire** to the common ground rail.
4. Solder a $470\,\mu\text{F} - 1000\,\mu\text{F}$ ($16\text{V}$ or $25\text{V}$) electrolytic capacitor directly across the servo's Red and Brown wires. This capacitor buffers the sudden $2.5\text{A}$ current surges when the servo changes direction, preventing Uno resets.

### Step 5: Star Ground Implementation
1. Dedicate a single screw terminal or copper ground block as the **Star Ground point**.
2. Connect the following ground returns directly to this point:
   * $12\text{V}$ Adapter negative terminal (`-`)
   * LM2596 `OUT-` (GND)
   * Arduino Uno `GND`
   * MG996R Servo Ground (Brown wire)
   * SimonK ESC Signal Ground (Thin Black wire)
   * SimonK ESC Power Ground (Thick Black wire)
   * MLX90614 GND, and HC-SR501 GND if the PIR backup is fitted

---

## 6. Diagnostic Sanity Verification Sketches

Before uploading the complete tracking algorithm, run these maintained test sketches (in
`firmware/arduino_ide/`) to test each subsystem in isolation. They replace the older inline
ESP32 snippets that used to be in this section.

| Sketch | What it proves | Pass looks like |
| :--- | :--- | :--- |
| `01_board_test` | Uno + IDE + USB work | LED blinks, `uptime=… freeRAM=…` prints |
| `02_mlx90614_sensor_test` | I2C wiring, pull-ups, sensor | `device ACKs at 0x5A`, `Ambient read: OK` with room temperature |
| `03_servo_continuous_test` | Servo stop pulse; spins both ways | Shaft still at the stop value, turns either side of it |
| `04_esc_fan_ramp_test` (**blades off**) | ESC arming and smooth throttle | Arming beeps, smooth ramps from `0`-`100` |
| `05_pir_test` (PIR backup only) | HC-SR501 on D2 | After 60 s warm-up: `MOTION` on a wave, `clear` ~3 s later |
| `06_head_sweep_test` | Full head motion | CW 0→360°, pause, CCW back to the start; cables never pull tight |

> [!IMPORTANT]
> **REMOVE FAN BLADES FROM THE MOTOR BEFORE RUNNING TEST 4.**

---

## 7. Pre-Flight Power-On Checklist

Before flipping the master power switch, check off every item:

- [ ] **Multimeter Check:** LM2596 `OUT+` is verified at **$5.05\text{V} - 5.10\text{V}$** under no load.
- [ ] **Sensor Voltage:** MLX90614 `VIN` connects to the Uno **3.3V** pin, **not** 5V or 12V.
- [ ] **Pull-Up Resistors:** A4 and A5 are pulled to **+3.3V** (not 5V), by the GY-906's on-board resistors or by two $4.7\text{k}\Omega$ resistors.
- [ ] **No Motor Driver:** No L298N / tilt motor; D7 and D8 are unconnected.
- [ ] **Uno Power:** 12 V goes only to Uno `VIN` (never to 5V or 3.3V); the LM2596 output is **not** connected to the Uno.
- [ ] **ESC Red Wire:** Center Red wire on the ESC receiver lead is **cut or insulated**.
- [ ] **Motor Phases:** Motor phase wires $U, V, W$ connect **exclusively** to the BLDC motor.
- [ ] **Decoupling Capacitor:** A $1000\,\mu\text{F}$ capacitor is installed across the MG996R servo power leads.
- [ ] **Common Ground:** All ground lines converge at the **Single Star Ground Point**.
- [ ] **Propeller Safety:** Fan blades are removed during initial bench test.

Once all items are checked, upload the complete tracking firmware from
`firmware/arduino_ide/follow_me_fan_full/` and follow [`ARDUINO_UNO_BUILD_GUIDE.md`](ARDUINO_UNO_BUILD_GUIDE.md).
