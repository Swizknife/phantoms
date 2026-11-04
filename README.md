# Phantoms: Follow-Me Thermal-Tracking Fan

An Arduino Uno pedestal fan that finds people by their body heat and points itself at them. It also sets the fan speed from how warm they are.

1. **Scan:** an MLX90614 IR sensor on the fan head is swept clockwise 0→360° and back anticlockwise by a continuous-rotation servo.
2. **Detect:** the firmware finds warm groups (people) in that 360° scan.
3. **Track:** the head swings back and forth over the arc where people are.
4. **Fan speed:** a BLDC fan, driven through an ESC, runs at one of 3 speed modes chosen from the person−ambient temperature difference.

If the MLX90614 isn't found at boot, the firmware automatically switches to an HC-SR501 PIR sensor as a backup.

## Hardware

| Part | Model | Role |
| :--- | :--- | :--- |
| Controller | Arduino Uno (ATmega328P) | Logic, two 50 Hz pulse outputs |
| Thermal sensor | MLX90614 (GY-906), I2C 0x5A | Detects people by body heat |
| Yaw servo | MG996R, 360° continuous | Turns the fan head |
| ESC + fan motor | SimonK 30A + A2212 1000 KV BLDC | Airflow |
| Buck converter | LM2596, set to 5.05 V | Servo power only |
| Power supply | 12 V 5 A adapter | Feeds everything |
| Backup sensor | HC-SR501 PIR | Used only if the MLX90614 fails |
| Passives | 1000 µF capacitor, 2 × 4.7 kΩ resistors | Servo decoupling; I2C pull-ups (skip them if the GY-906 board has its own) |

There is no tilt mechanism and no motor driver.

| Uno pin | Connects to |
| :--- | :--- |
| D9 | MG996R signal (orange) |
| D10 | ESC signal (white) |
| A4 / A5 | MLX90614 SDA / SCL |
| 3.3V | MLX90614 VIN |
| D2 / 5V | HC-SR501 OUT / VCC (backup only) |
| VIN / GND | 12 V adapter (+) / star ground |

The full wire-by-wire list is in [`COMPLETE_COMPONENT_CONNECTIONS_REFERENCE.md`](COMPLETE_COMPONENT_CONNECTIONS_REFERENCE.md).

## Opening the firmware

In the Arduino IDE, use **File → Open** and choose
`firmware/arduino_ide/follow_me_fan_full/follow_me_fan_full.ino`. The IDE then shows tabs for `config.h`, `scan.h`, `yaw.cpp` and the other files.

Keep all 13 files together in that folder. Copying or pasting only the `.ino` gives `config.h: No such file or directory` errors.

All settings, including pins, speeds, sweep direction (`YAW_REVERSE`) and PIR timing, are in `config.h`.

## Test order

Run the sketches in `firmware/arduino_ide/` in this order before the full firmware:

| # | Sketch | Checks |
| :-: | :--- | :--- |
| 1 | `01_board_test` | Board, USB and upload |
| 2 | `02_mlx90614_sensor_test` | Sensor answers at 0x5A and reads temperatures |
| 3 | `03_servo_continuous_test` | Servo stop pulse and direction (sets `YAW_REVERSE`) |
| 4 | `04_esc_fan_ramp_test` | ESC arming and smooth ramp. **Remove the fan blades first.** |
| 5 | `05_pir_test` | PIR backup on D2 (only if used) |
| 6 | `06_head_sweep_test` | Full clockwise 0→360°, anticlockwise back |

Then upload `follow_me_fan_full` and run `cal` in the Serial Monitor (115200 baud).

## Docs

- [`ARDUINO_UNO_BUILD_GUIDE.md`](ARDUINO_UNO_BUILD_GUIDE.md): step-by-step build and bench procedure
- [`firmware/README.md`](firmware/README.md): serial commands, calibration, tuning, and how PIR mode works
- [`firmware/arduino_ide/TEAMMATE_HANDOFF_GUIDE.md`](firmware/arduino_ide/TEAMMATE_HANDOFF_GUIDE.md): beginner walkthrough
- [`PRODUCT_HANDOFF_POWER_AND_CONTROL_NOW.md`](PRODUCT_HANDOFF_POWER_AND_CONTROL_NOW.md): power and control architecture
- [`bldc_speed_control_handoff.md`](bldc_speed_control_handoff.md): ESC and fan speed details
