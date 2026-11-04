# Complete Component-by-Component Interconnect Manual
## Atomberg Hackathon: Arduino Uno Thermal-Tracking "Follow-Me" Pedestal Fan
**Document Type:** Exhaustive Physical Wiring & Pin-to-Pin Interconnect Bible  
**Scope:** Every terminal, pin, wire color, gauge, and connection for all 9 hardware components.  
**Controller:** Arduino Uno (ATmega328P, 5 V logic). The earlier ESP32-S3 build was replaced; behaviour is unchanged.  
**Motors:** Only the MG996R servo (yaw) and the A2212 BLDC (fan). The tilt mechanism (L298N + DC motor) has been removed, so there is no motor driver.  
**See also:** [`ARDUINO_UNO_BUILD_GUIDE.md`](ARDUINO_UNO_BUILD_GUIDE.md) for the step-by-step bench procedure.

---

## Master Bill of Components

```
+----------------------------------------------------------------------------------------------------+
|                                    HACKATHON HARDWARE INVENTORY                                    |
+----+----------------------------------+------------------------------+-----------------------------+
| ID | Component Name                   | Model / Specification        | Primary Role                |
+----+----------------------------------+------------------------------+-----------------------------+
| C1 | Master Microcontroller           | Arduino Uno (ATmega328P)     | Perception, Logic, Dual PWM |
| C2 | Non-Contact Thermal Sensor       | Melexis MLX90614 (GY-906)    | Human IR Heat Detection     |
| C3 | Yaw Pan Actuator                 | TowerPro MG996R 360° (cont.) | CW 0->360°, then CCW back   |
| C4 | Fan Speed Controller (ESC)       | SimonK 30A Brushless ESC     | 3-Phase BLDC Commutation    |
| C5 | Primary Airflow Motor            | A2212 1000KV 3-Phase BLDC    | Main Cooling Airflow        |
| C6 | DC-DC Step-Down Buck Converter   | LM2596 Switching Regulator   | 12V -> 5.05V Regulated Rail |
| C7 | Main Power Supply                | 12V 5A DC Adapter (clips)    | System-Wide Raw DC Source   |
| C8 | Backup Presence Sensor (Optional)| HC-SR501 PIR Module          | Fallback if MLX90614 fails  |
| C9 | Passive & Protection Components  | 4.7kΩ Resistors & 1000uF Cap | Pull-ups & Surge Buffering  |
+----+----------------------------------+------------------------------+-----------------------------+
```

> [!NOTE]
> **Mounting:** the MLX90614 sits on the fan head (with the BLDC), and the MG996R rotates the whole head. The head turns at most one full turn before reversing, so cables crossing the head/base joint twist by one turn at most. No slip ring is needed: leave a 20-25 cm slack loop at the joint.

> [!WARNING]
> **USB power bank:** it gives only 5 V (about 2 A) and cannot run the ESC/BLDC or the servo. Use it, or laptop USB, only to power the Uno by itself for uploads and sensor bench tests.

---

## Component-by-Component Detailed Interconnects

---

### Component 1: Arduino Uno (Master Microcontroller)

```
                          ARDUINO UNO  (pins used by this project)

        DIGITAL HEADER (top, left to right)            POWER HEADER (bottom)
   +--------------------------------------------+    +-----------------------------+
   | D13 D12 D11 D10 D9  ...  D2  D1  D0        |    | IOREF RESET 3.3V 5V GND GND VIN |
   |      PWM  ^   ^          ^                 |    +-----------------------------+
   |           |   |          |                 |      (3.3V = sensor, 5V = PIR only,
   |        ESC | Servo      PIR OUT            |       VIN = 12V in)
   |      signal  signal    (backup)            |    ANALOG HEADER
   +--------------------------------------------+    +-----------------------------+
                                                     | A0 A1 A2 A3 [A4=SDA] [A5=SCL] |
                                                     +-----------------------------+
                         [USB-B]  [DC jack: leave empty]
```

#### Detailed Pin Connection Table:
| Uno Pin | Connects To (Destination) | Destination Terminal | Signal Function | Wire Spec |
| :--- | :--- | :--- | :--- | :--- |
| **3.3V** | **MLX90614** & **Pull-Ups** | `VIN` pin & two $4.7\text{k}\Omega$ resistors | $+3.3\text{V}$ Clean Sensor Power (about 1.5 mA) | Red (24 AWG) |
| **D9** | **MG996R Servo** | Orange Signal Wire | $50\text{Hz}$ pulse (Servo library, Timer1) | Orange (24 AWG)|
| **D10** | **SimonK 30A ESC** | White Signal Wire | $50\text{Hz}$ pulse, 1000-1350 µs (Servo library) | White (24 AWG) |
| **D2** | **HC-SR501 PIR** (backup only) | `OUT` Pin | Digital presence input (3.3 V high) | Green (24 AWG) |
| **A4 (SDA)** | **MLX90614 Sensor** | `SDA` Pin (Data) | I2C Serial Data ($4.7\text{k}\Omega$ to **3.3V**) | Blue (24 AWG) |
| **A5 (SCL)** | **MLX90614 Sensor** | `SCL` Pin (Clock) | I2C Serial Clock ($4.7\text{k}\Omega$ to **3.3V**)| Yellow (24 AWG)|
| **VIN** | **12V 5A Adapter (+)** | Positive terminal (via the star bus) | $+12\text{V}$ in; the Uno's own regulator makes its 5 V | Red (20 AWG) |
| **GND** (any) | **Central Star Ground** | Ground Junction Block | System Common Logic Ground | Black (20 AWG) |
| **5V pin** | **HC-SR501 PIR `VCC`** (backup only) | `VCC` Pin | PIR power (about 65 µA). Otherwise leave empty. Never feed the LM2596 into it; never power the servo from it | Red (24 AWG) |
| **D7 / D8** | **NOT USED** | — | Free (were the L298N tilt inputs, now removed) | — |
| **D0 / D1** | **NOT USED** | — | Reserved for USB serial | — |

> [!CAUTION]
> **The Uno is a 5 V board; the MLX90614 is a 3.3 V part.** The Uno's I2C pins have internal pull-ups to 5 V. The firmware switches them **off** (`thermal.cpp`), so the only pull-ups are the two external $4.7\text{k}\Omega$ resistors to **3.3V**. Do not tie those resistors to 5 V. The Uno reads a 3.3 V "high" correctly (threshold about 3.0 V).

> [!NOTE]
> **Powering the Uno:** the 12 V adapter goes to `VIN` (the on-board regulator accepts 7-12 V). For bench work you can power it from USB alone; the board switches sources automatically. Do **not** connect the LM2596 5 V output to the Uno `5V` pin while USB or `VIN` is also connected.

---

### Component 2: MLX90614 Infrared Thermal Sensor (GY-906 Breakout)

```
                       MLX90614 (GY-906 Module)
                       +----------------------+
                       |     [ TO-39 CAN ]    |
                       |                      |
                       |  [1]  [2]  [3]  [4]  |
                       +---|----|----|----|---+
                          VIN  GND  SCL  SDA
```

#### Detailed Pin Connection Table:
| Pin # | Pin Label | Connects To (Destination) | Destination Terminal | Signal Function | Wire Spec |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **1** | `VIN` | **Uno** & **Pull-Ups** | `3.3V` pin & two $4.7\text{k}\Omega$ | Sensor $+3.3\text{V}$ Power Rail | Red (24 AWG) |
| **2** | `GND` | **Central Star Ground** | Uno `GND` | Ground Reference | Black (24 AWG) |
| **3** | `SCL` | **Uno** | `A5` | I2C Clock (Tied to 3.3V via 4.7k) | Yellow (24 AWG)|
| **4** | `SDA` | **Uno** | `A4` | I2C Data (Tied to 3.3V via 4.7k) | Blue (24 AWG) |

> [!CAUTION]
> **NEVER connect a bare MLX90614 `VIN` to 5V or 12V.** It operates strictly on $+3.3\text{V}$. The two $4.7\text{k}\Omega$ pull-up resistors must tie to the $+3.3\text{V}$ rail, **never** to $5\text{V}$ or $12\text{V}$. (A GY-906 breakout with an on-board regulator tolerates 5 V on `VIN`, but keep 3.3 V for consistency.)

> [!IMPORTANT]
> **Check for on-board pull-ups first.** Many GY-906 boards with a regulator already have two small resistors next to SDA/SCL (marked `103` or `472`). If yours has them, **do not** add the external $4.7\text{k}\Omega$ resistors. Fit them only if the board has none.

> [!TIP]
> The sensor rides on the rotating head, so its 4 wires cross the head/base joint. Twist SDA and SCL each with a GND wire and keep the run under about 50 cm.

---

### Component 3: TowerPro MG996R Metal-Gear Servo Motor (Yaw Actuator)

```
                            MG996R SERVO MOTOR
                            +-----------------+
                            |   [GEARBOX]     |
                            |   [MOTOR]       |
                            +--------+--------+
                                     | 3-Pin Cable
                                     |
                          +----------+----------+
                          |   [ ]    [ ]    [ ] |
                          +----|------|------|--+
                             Orange  Red   Brown
                             Signal  VCC    GND
```

#### Detailed Pin Connection Table:
| Wire Color | Terminal Role | Connects To (Destination) | Destination Terminal | Electrical Specs | Wire Spec |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Orange** | `PWM Signal` | **Uno** | `D9` | 5 V TTL, $50\text{Hz}$ pulse | Stock Servo Lead |
| **Red** | `VCC (+5V)` | **LM2596 Buck Converter** | `OUT+` Terminal | $+5.05\text{V}$ DC (Peak $2.5\text{A}$ stall)| 20 AWG Extension |
| **Brown** | `GND (0V)` | **Central Star Ground** | `OUT-` / Star GND block | Power Return Ground | 20 AWG Extension |
| **Capacitor**| $1000\,\mu\text{F}$ Buffer| **Across Red & Brown** | Directly at servo connector | Voltage sag & noise protection | Solder directly |

> [!WARNING]
> **DO NOT power the MG996R from the Uno.** A servo drawing $2.5\text{A}$ stall current will pull down the Uno's 5 V rail (its regulator and USB fuse supply well under 1 A) and cause brownout resets. Connect servo `Red` directly to the LM2596 `OUT+` terminal.

---

### Component 4: SimonK 30A Brushless ESC (Fan Speed Controller)

```
                           SIMONK 30A BLDC ESC
                +---------------------------------------+
(12V Raw Input) | Thick Red Wire   (+) -----------------|====> +12V DC Adapter (+)
                | Thick Black Wire (-) -----------------|====> Central Star Ground
                |                                       |
  (3-Phase AC   | Blue Wire (Phase U) ------------------|====> BLDC Motor Lead 1
     Output)    | Blue Wire (Phase V) ------------------|====> BLDC Motor Lead 2
                | Blue Wire (Phase W) ------------------|====> BLDC Motor Lead 3
                |                                       |
 (3-Wire Signal | White Wire (Signal) ------------------|====> Uno D10
     Header)    | Red Wire (5V BEC)   ------------------|====> [NC - TAPE OFF!]
                | Black Wire (GND)    ------------------|====> Central Star Ground
                +---------------------------------------+
```

#### Detailed Pin Connection Table:
| Lead / Wire | Wire Description | Connects To (Destination) | Destination Terminal | Signal Function | Wire Gauge |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Thick Red** | Main Power (+) | **12V 5A Power Adapter** | Positive (+) Terminal | $+12.0\text{V}$ Raw Power Bus | 14 - 16 AWG |
| **Thick Black**| Main Ground (-) | **Central Star Ground** | Adapter Negative (-) | High-Current Motor Return | 14 - 16 AWG |
| **Motor Blue 1**| Phase Output U | **12V BLDC Fan Motor** | Motor Wire 1 (Phase U) | 3-Phase AC Stator Drive | 16 AWG |
| **Motor Blue 2**| Phase Output V | **12V BLDC Fan Motor** | Motor Wire 2 (Phase V) | 3-Phase AC Stator Drive | 16 AWG |
| **Motor Blue 3**| Phase Output W | **12V BLDC Fan Motor** | Motor Wire 3 (Phase W) | 3-Phase AC Stator Drive | 16 AWG |
| **Thin White** | Throttle PWM In | **Uno** | `D10` | $50\text{Hz}$ pulse (firmware caps at $1350\mu\text{s}$)| 24 AWG |
| **Thin Black** | Signal Ground | **Central Star Ground** | Uno `GND` / Star Bus | PWM Ground Reference | 24 AWG |
| **Thin Red** | Internal 5V BEC | **NO CONNECTION** | **TAPE OFF & INSULATE**| DO NOT CONNECT TO 5V RAIL! | Cut / Insulated |

> [!CAUTION]
> **INSULATE THE THIN RED WIRE:** The SimonK ESC has an internal linear BEC outputting $5\text{V}$. If this wire touches the LM2596 $+5.05\text{V}$ rail, circular currents will fight the buck regulator and burn out the BEC.

---

### Component 5: A2212 1000KV 3-Phase BLDC Fan Motor (Primary Airflow)

```
                         A2212 1000KV BLDC FAN MOTOR
                            +----------------+
                            |  [FAN HUB &]   |
                            |  [ BLADES  ]   |
                            +---+---+---+----+
                                |   |   |
                                U   V   W
                                (3 Stator Wires)
```

#### Detailed Pin Connection Table:
| Motor Wire | Connects To (Destination) | Destination Terminal | Notes |
| :--- | :--- | :--- | :--- |
| **Wire 1 (Phase U)** | **SimonK 30A ESC** | Blue Phase U Wire | Solder or $3.5\text{mm}$ bullet connector |
| **Wire 2 (Phase V)** | **SimonK 30A ESC** | Blue Phase V Wire | Solder or $3.5\text{mm}$ bullet connector |
| **Wire 3 (Phase W)** | **SimonK 30A ESC** | Blue Phase W Wire | Solder or $3.5\text{mm}$ bullet connector |

> [!TIP]
> **Fan Direction Reversal:** If the motor spins backwards (blows air backward), simply swap any **two** of the three blue phase wires (e.g., swap Wire 1 and Wire 2).

---

### Component 6: LM2596 DC-DC Step-Down Buck Converter

```
                             LM2596 BUCK CONVERTER
                        +-----------------------------+
(From 12V Adapter) ===> | IN+                     OUT+| ===> [+5.05V to the SERVO only]
(To Star Ground)   ===> | IN-                     OUT-| ===> [Common Star Ground]
                        |         [TRIMPOT]           |
                        +-----------------------------+
```

#### Detailed Pin Connection Table:
| Terminal | Connects To (Source/Destination) | Electrical Specification | Notes |
| :--- | :--- | :--- | :--- |
| **`IN+`** | **12V 5A Power Adapter (+)** | $+12.0\text{V}$ Raw Input | 18 AWG Wire |
| **`IN-`** | **12V 5A Power Adapter (-)** | $0\text{V}$ Input Ground | 18 AWG Wire |
| **`OUT+`** | **MG996R `Red`** & capacitor `+` | **$+5.05\text{V}$ Regulated DC** | Must be pre-calibrated with DMM. Not connected to the Uno. |
| **`OUT-`** | **Central Star Ground Bus** | $0\text{V}$ Common Ground | 18 AWG Wire |

---

### Component 7: 12V 5A DC Power Adapter (Master System Source)

The adapter output ends in **alligator clips**. Do not clip them onto bundles of wires. Clip them onto two short 16 AWG stub wires screwed into a 2-row terminal block: one row is the **+12V bus**, the other is the **Central Star Ground**.

#### Detailed Pin Connection Table:
| Terminal / Jack | Connects To (Destination) | Wire Gauge | Notes |
| :--- | :--- | :--- | :--- |
| **Positive (+12V, red clip)**| +12V bus → **LM2596 `IN+`**, **SimonK ESC `Thick Red`**, **Uno `VIN`** | 14 - 16 AWG | Heavy master positive bus |
| **Negative (GND, black clip)** | **Central System Star Ground Point** | 14 - 16 AWG | Heavy master negative return |

> [!CAUTION]
> Clip red to the +12V stub and black to the GND stub **before** plugging the adapter into the wall, and make sure the two clips can't touch each other.

---

### Component 8: HC-SR501 PIR Motion Sensor (Backup, only if the MLX90614 fails)

```
                        HC-SR501 PIR (pins underneath)
                        +----------------------------+
                        |        ( Fresnel dome )    |
                        |  [TIME pot]   [SENS pot]   |
                        |   [VCC]  [OUT]  [GND]      |
                        +----|------|------|---------+
                           Uno 5V  Uno D2  Uno GND
```

#### Detailed Pin Connection Table:
| Pin | Connects To (Destination) | Destination Terminal | Signal Function | Wire Spec |
| :--- | :--- | :--- | :--- | :--- |
| **`VCC`** | **Uno** | `5V` pin | Module power (accepts 4.5-20 V) | Red (24 AWG) |
| **`OUT`** | **Uno** | `D2` | 3.3 V high = motion detected | Green (24 AWG) |
| **`GND`** | **Central Star Ground** | Uno `GND` | Ground Reference | Black (24 AWG) |

**Settings:** trigger jumper on **H** (repeat trigger), TIME pot fully anticlockwise (about 3 s), SENS pot in the middle. Allow about 60 s warm-up after power-on.

> [!WARNING]
> A PIR senses **changes** in IR, so it false-triggers while the head is rotating. Mount it **on the rotating head** in place of the MLX90614, facing the same way as the fan. The firmware handles the false triggers: when no MLX90614 answers at boot, it switches to the PIR automatically. It then turns the head in 30° steps and, at each step, waits for OUT to drop before watching it (`pir.cpp`). The fan runs at mode 2 when someone is detected. Test the PIR first with `05_pir_test`.

---

### Component 9: Passive & Protection Components

#### 1. I2C Bus Pull-Up Resistors (Dual $4.7\text{k}\Omega$ Metal Film; skip if the GY-906 already has on-board pull-ups)
* **Resistor 1 (SDA Pull-Up):**
  * Leg A: Connects to Uno `A4` / MLX90614 `SDA`.
  * Leg B: Connects to the Uno `3.3V` pin (the 3.3 V rail).
* **Resistor 2 (SCL Pull-Up):**
  * Leg A: Connects to Uno `A5` / MLX90614 `SCL`.
  * Leg B: Connects to the Uno `3.3V` pin (the 3.3 V rail).

#### 2. Servo Surge Buffer Capacitor ($1000\,\mu\text{F}, 16\text{V}$ Low-ESR Electrolytic)
* **Positive Leg (+ Longer Lead):** Connects to MG996R `Red` Wire (+5.05V rail).
* **Negative Leg (- Striped Lead):** Connects to MG996R `Brown` Wire (Ground).
* *Purpose:* Eliminates the $2.5\text{A}$ inductive voltage sag when the servo reverses direction.

---

## 3. Master System Netlist (Point-to-Point Node Table)

This netlist traces every electrical node across the entire prototype:

```
+----------------------------------------------------------------------------------------------------+
|                                      SYSTEM MASTER NETLIST                                         |
+--------------------+-------------------------------------------------------------------------------+
| Net / Node Name    | Connected Component Pins / Terminals                                          |
+--------------------+-------------------------------------------------------------------------------+
| NET_12V_RAW        | 1. 12V 5A Adapter (+)                                                         |
|                    | 2. LM2596 Buck IN+                                                            |
|                    | 3. SimonK 30A ESC Thick Red Wire                                              |
|                    | 4. Arduino Uno VIN pin                                                        |
+--------------------+-------------------------------------------------------------------------------+
| NET_5V_SERVO       | 1. LM2596 Buck OUT+ (+5.05V)                                                  |
|                    | 2. MG996R Servo Red Wire (VCC)                                                |
|                    | 3. 1000uF Electrolytic Capacitor Positive (+) Terminal                        |
|                    | (NOT connected to the Uno 5V pin)                                             |
+--------------------+-------------------------------------------------------------------------------+
| NET_3V3_CLEAN      | 1. Arduino Uno 3.3V pin                                                       |
|                    | 2. MLX90614 Sensor Pin 1 (VIN)                                                |
|                    | 3. Resistor R1 Leg B (4.7k Pull-up for SDA)                                   |
|                    | 4. Resistor R2 Leg B (4.7k Pull-up for SCL)                                   |
+--------------------+-------------------------------------------------------------------------------+
| NET_STAR_GND (0V)  | 1. 12V 5A Adapter (-)                                                         |
|                    | 2. LM2596 Buck IN-                                                            |
|                    | 3. LM2596 Buck OUT-                                                           |
|                    | 4. Arduino Uno GND (any GND pin)                                              |
|                    | 5. SimonK 30A ESC Thick Black Wire (Power GND)                                |
|                    | 6. SimonK 30A ESC Thin Black Wire (Signal GND)                                |
|                    | 7. MG996R Servo Brown Wire (GND)                                              |
|                    | 8. 1000uF Electrolytic Capacitor Negative (-) Terminal                        |
|                    | 9. MLX90614 Sensor Pin 2 (GND)                                                |
|                    | 10. HC-SR501 PIR GND (backup only)                                            |
+--------------------+-------------------------------------------------------------------------------+
| NET_5V_UNO         | 1. Arduino Uno 5V pin                                                         |
|                    | 2. HC-SR501 PIR VCC (backup only; otherwise this pin is left empty)           |
+--------------------+-------------------------------------------------------------------------------+
| SIG_SERVO_PWM      | 1. Arduino Uno D9                                                             |
|                    | 2. MG996R Servo Orange Wire (PWM Signal)                                      |
+--------------------+-------------------------------------------------------------------------------+
| SIG_ESC_PWM        | 1. Arduino Uno D10                                                            |
|                    | 2. SimonK 30A ESC Thin White Wire (PWM Signal)                                |
+--------------------+-------------------------------------------------------------------------------+
| SIG_I2C_SDA        | 1. Arduino Uno A4 (SDA)                                                       |
|                    | 2. MLX90614 Sensor Pin 4 (SDA)                                                |
|                    | 3. Resistor R1 Leg A (4.7k Pull-up)                                           |
+--------------------+-------------------------------------------------------------------------------+
| SIG_I2C_SCL        | 1. Arduino Uno A5 (SCL)                                                       |
|                    | 2. MLX90614 Sensor Pin 3 (SCL)                                                |
|                    | 3. Resistor R2 Leg A (4.7k Pull-up)                                           |
+--------------------+-------------------------------------------------------------------------------+
| SIG_PIR_OUT        | 1. Arduino Uno D2                                                             |
|                    | 2. HC-SR501 PIR OUT (backup only)                                             |
+--------------------+-------------------------------------------------------------------------------+
| NET_MOTOR_PHASE_U  | 1. SimonK 30A ESC Blue Phase U Wire                                           |
|                    | 2. 12V BLDC Fan Motor Wire 1                                                  |
+--------------------+-------------------------------------------------------------------------------+
| NET_MOTOR_PHASE_V  | 1. SimonK 30A ESC Blue Phase V Wire                                           |
|                    | 2. 12V BLDC Fan Motor Wire 2                                                  |
+--------------------+-------------------------------------------------------------------------------+
| NET_MOTOR_PHASE_W  | 1. SimonK 30A ESC Blue Phase W Wire                                           |
|                    | 2. 12V BLDC Fan Motor Wire 3                                                  |
+--------------------+-------------------------------------------------------------------------------+
```

---

## 4. Master Visual Wiring Diagram

```
====================================================================================================
                                 COMPLETE SYSTEM WIRING DIAGRAM
====================================================================================================

         +12V 5A DC POWER SUPPLY
        +-----------------------+
        |   (+)           (-)   |
        +----|-------------|----+
             |             |
             +-------------|---------------------------+
             |             |                           |
             +--------+    |                           |
             |        |    |                           |
             v (+12V) v    |                           v (+12V)
        +----------+ +---------+                 +------------+
        |  LM2596  | | ARDUINO |                 | SimonK 30A |
        |   BUCK   | |   UNO   |                 |  BLDC ESC  |
        | STEPDOWN | |  (VIN)  |                 +-----+------+
        +----+-----+ +----+----+                       |
             |            |                            | 3-Phase AC (U, V, W)
             | +5.05V     | 3.3V pin      5V pin       v
             | (servo     v                 |    [A2212 BLDC FAN]
             |  only)  +----------+         v
             v         | MLX90614 |   +-----------+
        +----------+   |  SENSOR  |   | HC-SR501  |
        |  MG996R  |   +----+-----+   | PIR (bkp) |
        |  SERVO   |        |         +-----+-----+
        | (+1000uF)|        |               |
        +----+-----+        |               |
             |              |               |
   ==========v==============v===============v=========================================
                                    SYSTEM COMMON STAR GROUND (0V)
   ===================================================================================
   (Uno GND, LM2596 IN-/OUT-, ESC black wires, servo brown, sensor GNDs, adapter (-) all meet here.)

   SIGNAL INTERCONNECTS:
   * Uno D9   ========[50Hz Servo pulse]=====> MG996R Servo Orange Wire
   * Uno D10  ========[50Hz ESC pulse]=======> SimonK ESC Thin White Wire
   * Uno D2   <=======[PIR motion (3.3V)]===== HC-SR501 OUT        (backup only)
   * Uno A4   <=======[I2C Data (SDA)]=======> MLX90614 SDA (Pin 4) (+4.7k to 3.3V)
   * Uno A5   ========[I2C Clock (SCL)]======> MLX90614 SCL (Pin 3) (+4.7k to 3.3V)
   * SimonK ESC Red Wire ====[5V BEC Unused]=> TAPE OFF & INSULATE (NC)
```

---

## 5. Sequential Step-by-Step Assembly Protocol

Follow this exact assembly order during hackathon integration:

1. **Phase 1: Power Rail Verification (Stand-alone)**
   * Connect $12\text{V}$ power supply to LM2596 `IN+` and `IN-`.
   * Measure LM2596 `OUT+` and `OUT-` with a digital multimeter.
   * Adjust brass screw until multimeter reads **exactly $+5.05\text{V}$**.
   * Turn off power.
2. **Phase 2: Common Star Ground Bus Setup**
   * Establish a central terminal block for the Star Ground.
   * Connect $12\text{V}$ Adapter (-), LM2596 `IN-`, LM2596 `OUT-`, and one Uno `GND` pin to this block.
3. **Phase 3: Sensor Integration**
   * Wire MLX90614 `VIN` to the Uno `3.3V` pin.
   * Wire MLX90614 `GND` to the Uno `GND`.
   * Connect `SDA` to `A4` and `SCL` to `A5`.
   * Solder the two $4.7\text{k}\Omega$ resistors between `SDA` $\to$ `3.3V` and `SCL` $\to$ `3.3V`.
   * Power the Uno via USB and upload `02_mlx90614_sensor_test` to verify address `0x5A`.
4. **Phase 4: Servo Panning Mechanism**
   * Connect servo `Orange` wire to Uno `D9`.
   * Connect servo `Red` wire to LM2596 `OUT+` ($+5.05\text{V}$).
   * Connect servo `Brown` wire to Central Star Ground.
   * Solder $1000\,\mu\text{F}$ capacitor across Red and Brown wires.
   * Upload `03_servo_continuous_test` to find the stop pulse and verify both spin directions. If stop−60 µs turns the head anticlockwise, set `YAW_REVERSE = true` in `config.h`.
5. **Phase 5: SimonK ESC & BLDC Fan (SAFETY FIRST)**
   * **REMOVE FAN BLADES FROM MOTOR.**
   * Slide out and tape off the SimonK thin Red BEC wire.
   * Connect SimonK Thick Red to $+12\text{V}$ and Thick Black to Central Star Ground.
   * Connect SimonK Thin White to Uno `D10` and Thin Black to Central Star Ground.
   * Connect 3 Blue motor wires to the BLDC motor.
   * Power up and upload `04_esc_fan_ramp_test`. Listen for the 3 rising initialization beeps.
   * Verify motor rotation direction. If reversed, swap any two blue motor wires.
   * Reattach fan blades only after software safety checks pass.
6. **Phase 6: Head Cabling**
   * Route the MLX90614 wires (and the BLDC or ESC leads, depending on where the ESC is mounted) through the head/base joint with a 20-25 cm slack loop.
   * Rotate the head by hand one full turn each way and check that nothing pulls tight.
   * Upload `06_head_sweep_test` and type `g`: the head should turn clockwise 0→360°, pause, then turn anticlockwise back to the start.
7. **Phase 7 (only if the MLX90614 fails): PIR Backup**
   * Remove the MLX90614 and mount the HC-SR501 on the head in its place.
   * Wire HC-SR501 `VCC` to Uno `5V`, `OUT` to `D2`, and `GND` to Star Ground.
   * Set the jumper to H, TIME to minimum, and SENS to the middle.
   * Upload `05_pir_test` and confirm `MOTION`/`clear` after the 60 s warm-up.
   * Upload `follow_me_fan_full` unchanged: with no MLX90614 present it uses the PIR automatically (`status` shows `sensor=PIR`).
