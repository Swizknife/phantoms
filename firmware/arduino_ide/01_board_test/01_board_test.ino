/*
  TEST 1: Arduino Uno board test.
  Goal: confirm the IDE can upload to the board and the board is alive. No wiring needed.

  Expected: the on-board LED (D13, marked "L") blinks once per second, and the Serial Monitor
  (115200 baud) prints a line per second with uptime, free RAM and the CPU clock.
  If upload fails: check Tools > Board = Arduino Uno, the right COM port, and the USB cable.
  Clone boards with a CH340 chip need the CH340 driver installed.
*/

static int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println(F("\n=== Uno board test starting ==="));
}

void loop() {
  static bool on = false;
  on = !on;
  digitalWrite(LED_BUILTIN, on);

  Serial.print(F("uptime="));
  Serial.print(millis() / 1000);
  Serial.print(F("s  freeRAM="));
  Serial.print(freeRam());
  Serial.print(F("B  cpu="));
  Serial.print(F_CPU / 1000000UL);
  Serial.println(F("MHz"));
  delay(1000);
}
