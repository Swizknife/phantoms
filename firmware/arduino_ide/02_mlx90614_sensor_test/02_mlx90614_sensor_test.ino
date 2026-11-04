/*
  TEST 2: MLX90614 thermal sensor -- DIAGNOSTIC VERSION (Arduino Uno).
  Prints the bus idle levels, a full address scan, and a raw breakdown of every read attempt on
  EVERY loop, so you never need to scroll back to see what happened at boot.

  IMPORTANT: if this sketch was already open in the Arduino IDE, CLOSE the tab and reopen the
  .ino from disk before uploading, or the IDE may save its old in-memory copy over this file.

  Wiring (see ARDUINO_UNO_BUILD_GUIDE.md):
    MLX90614 VIN  -> Uno 3.3V pin    (NEVER 5V or 12V on a bare 3.3 V chip)
    MLX90614 GND  -> Uno GND
    MLX90614 SDA  -> Uno A4
    MLX90614 SCL  -> Uno A5
    Pull-ups: if your GY-906 board already has two small resistors next to SDA/SCL (marked 103
    or 472), use those and add nothing. Otherwise add:
      4.7k resistor between SDA and 3.3V
      4.7k resistor between SCL and 3.3V
  The Uno's own pull-ups go to 5 V, so this sketch switches them OFF. Without 3.3V pull-ups
  (on-board or external) the bus will not work -- the "Bus idle levels" line below tells you.
*/

#include <Wire.h>

constexpr uint8_t SDA_PIN = A4;
constexpr uint8_t SCL_PIN = A5;
constexpr uint8_t MLX_ADDR = 0x5A;
constexpr uint8_t REG_TA = 0x06;
constexpr uint8_t REG_TOBJ = 0x07;

// Plain constants instead of `enum class`: the Arduino IDE generates function prototypes before
// it sees types declared further down, so a custom enum in a signature breaks compilation.
constexpr uint8_t STAGE_OK = 0;
constexpr uint8_t STAGE_WRITE_FAIL = 1;
constexpr uint8_t STAGE_REQUEST_FAIL = 2;
constexpr uint8_t STAGE_PEC_FAIL = 3;
constexpr uint8_t STAGE_ERROR_FLAG = 4;

uint8_t crc8(uint8_t crc, uint8_t data) {
  crc ^= data;
  for (uint8_t i = 0; i < 8; i++)
    crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  return crc;
}

void printHex2(uint8_t v) {
  Serial.print(F("0x"));
  if (v < 16) Serial.print('0');
  Serial.print(v, HEX);
}

void startWire() {
  Wire.begin();
  Wire.setClock(100000);
  Wire.setWireTimeout(25000, true);  // never hang on a stuck bus
  digitalWrite(SDA_PIN, LOW);        // internal pull-ups off: only the external 3.3 V ones
  digitalWrite(SCL_PIN, LOW);
}

// Fills in every intermediate value so we can see exactly which step fails.
uint8_t readTempCVerbose(uint8_t reg, float &outC, uint8_t &writeStatus, uint8_t &gotBytes,
                         uint8_t &lo, uint8_t &hi, uint8_t &pecGot, uint8_t &pecCalc) {
  Wire.beginTransmission(MLX_ADDR);
  Wire.write(reg);
  writeStatus = Wire.endTransmission(false);  // repeated start, no stop
  if (writeStatus != 0) return STAGE_WRITE_FAIL;

  gotBytes = Wire.requestFrom(MLX_ADDR, (uint8_t)3);
  if (gotBytes != 3) return STAGE_REQUEST_FAIL;

  lo = Wire.read();
  hi = Wire.read();
  pecGot = Wire.read();

  uint8_t c = 0;
  c = crc8(c, MLX_ADDR << 1);
  c = crc8(c, reg);
  c = crc8(c, (MLX_ADDR << 1) | 1);
  c = crc8(c, lo);
  c = crc8(c, hi);
  pecCalc = c;
  if (pecCalc != pecGot) return STAGE_PEC_FAIL;

  uint16_t raw = ((uint16_t)hi << 8) | lo;
  if (raw & 0x8000) return STAGE_ERROR_FLAG;
  outC = raw * 0.02f - 273.15f;
  return STAGE_OK;
}

void printStage(uint8_t s) {
  switch (s) {
    case STAGE_OK: Serial.println(F("OK")); break;
    case STAGE_WRITE_FAIL: Serial.println(F("WRITE_FAIL (no ACK on register write -- device not responding)")); break;
    case STAGE_REQUEST_FAIL: Serial.println(F("REQUEST_FAIL (didn't get 3 bytes back)")); break;
    case STAGE_PEC_FAIL: Serial.println(F("PEC_FAIL (bytes received, but checksum doesn't match -- noisy/bad bus)")); break;
    case STAGE_ERROR_FLAG: Serial.println(F("ERROR_FLAG (sensor itself reports an internal error)")); break;
  }
}

// Release the I2C pins and read them as plain inputs (no internal pull-up). With working
// external pull-ups and nothing holding the bus, both must read HIGH. Then send the SMBus
// "wake" pulse some clone modules need, and restart I2C.
void checkBusLines() {
  Wire.end();
  pinMode(SDA_PIN, INPUT);
  pinMode(SCL_PIN, INPUT);
  delay(2);
  const bool sda = digitalRead(SDA_PIN);
  const bool scl = digitalRead(SCL_PIN);
  Serial.println(F("---- Bus idle levels ----"));
  Serial.print(F("  SDA(A4)="));
  Serial.print(sda ? F("HIGH") : F("LOW"));
  Serial.print(F("  SCL(A5)="));
  Serial.println(scl ? F("HIGH") : F("LOW"));
  if (!sda || !scl) {
    Serial.println(F("  A line is LOW at idle: no pull-up (board has none and no 4.7k fitted?),"));
    Serial.println(F("  pull-up not on 3.3V, line shorted to GND,"));
    Serial.println(F("  or sensor unpowered (an unpowered chip can clamp the lines low)."));
  }

  // Holding SCL low for >1.44 ms forces an MLX90614 stuck in PWM output mode back to SMBus.
  pinMode(SCL_PIN, OUTPUT);
  digitalWrite(SCL_PIN, LOW);
  delay(3);
  pinMode(SCL_PIN, INPUT);
  delay(2);

  startWire();
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== MLX90614 diagnostic sketch starting (Arduino Uno) ==="));
  startWire();
}

void loop() {
  checkBusLines();

  Serial.println(F("---- I2C scan ----"));
  bool found = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  device ACKs at "));
      printHex2(addr);
      Serial.println();
      if (addr == MLX_ADDR) found = true;
    }
  }
  if (!found) {
    Serial.println(F("  NOTHING acks at 0x5A. This is a wiring/power problem,"));
    Serial.println(F("  not a checksum problem -- check VIN=3.3V, GND, A4/A5 wires,"));
    Serial.println(F("  and that the pull-ups (on-board or 4.7k) go to 3.3V (not 5V)."));
    Serial.println(F("  If the MLX90614 is dead, the main firmware falls back to the PIR on D2"));
    Serial.println(F("  (test it with 05_pir_test)."));
  }

  float ta = 0;
  uint8_t writeStatus = 0, gotBytes = 0, lo = 0, hi = 0, pecGot = 0, pecCalc = 0;
  const uint8_t s = readTempCVerbose(REG_TA, ta, writeStatus, gotBytes, lo, hi, pecGot, pecCalc);
  Serial.print(F("Ambient read: "));
  printStage(s);
  Serial.print(F("  endTransmission()="));
  Serial.print(writeStatus);
  Serial.print(F("  bytesReceived="));
  Serial.print(gotBytes);
  Serial.print(F("  lo="));
  printHex2(lo);
  Serial.print(F(" hi="));
  printHex2(hi);
  Serial.print(F("  pecReceived="));
  printHex2(pecGot);
  Serial.print(F(" pecCalculated="));
  printHex2(pecCalc);
  Serial.println();

  if (s == STAGE_OK) {
    Serial.print(F("  -> Ambient = "));
    Serial.print(ta, 2);
    Serial.println(F(" C"));
    float to = 0;
    if (readTempCVerbose(REG_TOBJ, to, writeStatus, gotBytes, lo, hi, pecGot, pecCalc) == STAGE_OK) {
      Serial.print(F("  -> Object  = "));
      Serial.print(to, 2);
      Serial.println(F(" C"));
    }
  }

  Serial.println();
  delay(1000);
}
