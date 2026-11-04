#pragma once
#include <Arduino.h>

// Every tunable value lives here. Target board: Arduino Uno (ATmega328P, 5 V logic).
namespace cfg {

// ---------- Pins ----------
constexpr uint8_t PIN_SERVO = 9;    // MG996R 360 signal (orange)  -- Servo library, Timer1
constexpr uint8_t PIN_ESC   = 10;   // SimonK 30A signal (white)   -- Servo library, Timer1
constexpr uint8_t PIN_SDA   = A4;   // MLX90614 SDA (fixed hardware I2C pin)
constexpr uint8_t PIN_SCL   = A5;   // MLX90614 SCL (fixed hardware I2C pin)
constexpr uint8_t PIN_PIR   = 2;    // HC-SR501 OUT -- backup sensor, used only if no MLX90614 at boot
// No tilt / motor driver: the only motors are the servo (D9) and the BLDC via the ESC (D10).
constexpr uint32_t I2C_HZ   = 100000;
constexpr uint32_t I2C_TIMEOUT_US = 25000;  // a stuck bus must not freeze the control loop
constexpr uint8_t MLX_ADDR  = 0x5A;

// ---------- 50 Hz RC pulses (Servo library) ----------
constexpr uint16_t PULSE_ATTACH_MIN_US = 500;
constexpr uint16_t PULSE_ATTACH_MAX_US = 2500;

// ---------- Yaw: continuous-rotation servo, dead reckoning ----------
// "Forward" = angle increases = CLOCKWISE seen from above: the head sweeps CW 0->360, then CCW
// back. Forward normally gets (stop - SERVO_SCAN_OFFSET_US). If your head turns ANTICLOCKWISE
// during the forward sweep, set YAW_REVERSE = true (then forward gets stop + offset).
constexpr bool     YAW_REVERSE = false;
constexpr uint16_t SERVO_STOP_US_DEFAULT = 1500;  // trimmed by `cal`, stored in EEPROM
constexpr uint16_t SERVO_SCAN_OFFSET_US  = 60;    // pulse offset from stop used for all motion
constexpr float    OMEGA_DEFAULT_DEG_S   = 30.0f; // replaced by measured value after `cal`
constexpr float    YAW_MIN_DEG = 0.0f;
constexpr float    YAW_MAX_DEG = 360.0f;          // one turn max: cables must not twist further
constexpr uint32_t CAL_SPIN_MS = 10000;

// ---------- Thermal sensor ----------
constexpr uint32_t SENSOR_PERIOD_MS  = 50;     // 20 Hz
constexpr uint32_t SENSOR_FAULT_MS   = 1000;   // no valid read this long -> fault
constexpr float    AMBIENT_EMA_ALPHA = 0.05f;

// ---------- Scan analysis ----------
constexpr int   BIN_DEG  = 10;
constexpr int   NUM_BINS = 36;                 // 360 / 10
constexpr float RISE_THRESH_C    = 0.8f;       // bin above baseline by this -> part of a warm group
constexpr float PERSON_DELTA_C   = 1.0f;       // group peak - ambient >= this -> person
constexpr float FOV_EFF_DEG      = 90.0f;      // effective sensor FOV used for edge correction
constexpr float SWING_MARGIN_DEG = 15.0f;

// ---------- Speed modes (delta = person Tobj - ambient) ----------
constexpr float MODE_UP_2   = 2.0f;
constexpr float MODE_UP_3   = 4.0f;
constexpr float MODE_DOWN_1 = 1.5f;
constexpr float MODE_DOWN_2 = 3.5f;
constexpr uint32_t MODE_MIN_DWELL_MS = 10000;

// ---------- ESC ----------
constexpr uint16_t ESC_MIN_US = 1000;
constexpr uint16_t ESC_MAX_US = 1350;          // hard cap: 12 V 5 A supply shared with servo
constexpr uint16_t MODE_PULSE_US[4] = {1000, 1150, 1250, 1350};  // off, mode 1, 2, 3
constexpr uint32_t ESC_ARM_MS = 3000;
constexpr float    FAN_SLEW_US_PER_S = 140.0f; // 40 %/s of the 350 us range
constexpr uint32_t FAN_UPDATE_MS = 20;

// ---------- Behaviour ----------
constexpr uint32_t IDLE_RESCAN_MS  = 10000;    // nobody found: look again every 10 s
constexpr uint32_t TRACK_RESCAN_MS = 60000;    // while tracking: full 360 deg rescan every 60 s
constexpr uint8_t  LOST_PASSES     = 2;        // this many empty swings in a row -> rescan

// ---------- PIR backup (HC-SR501 on the head; auto-selected when no MLX90614 answers) ----------
// A PIR false-triggers while it moves, so the head turns in steps and stops before each reading.
// It gives no temperature, so a detected person always gets PIR_FAN_MODE.
constexpr uint32_t PIR_WARMUP_MS     = 60000;  // HC-SR501 output is unreliable for ~60 s after power-on
constexpr float    PIR_STEP_DEG      = 30.0f;  // scan stops at 0, 30, ... 360
constexpr uint32_t PIR_SETTLE_MIN_MS = 500;    // let the head stop vibrating
constexpr uint32_t PIR_SETTLE_MAX_MS = 6000;   // wait for OUT to drop after self-triggering (hold time ~3 s)
constexpr uint32_t PIR_WATCH_MS      = 2500;   // then any HIGH in this window = person here
constexpr uint8_t  PIR_FAN_MODE      = 2;
constexpr uint8_t  PIR_LOST_PASSES   = 6;      // a still person stops triggering, so be patient
constexpr uint32_t PIR_TRACK_RESCAN_MS = 180000;  // a stepped scan takes 1-2 min, so rescan less often

}  // namespace cfg
