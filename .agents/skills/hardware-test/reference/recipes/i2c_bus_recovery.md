# I2C bus recovery with a device stuck on purpose (#22852)

`scripts/i2c_recovery/`: `i2crec.yaml` (package that plants a wedge, see its header), per-run files `vl53l1x.yaml` and
`qwiic_pir.yaml` (Adafruit Feather HUZZAH, SDA GPIO4, SCL GPIO5), `i2cidf.yaml` (ESP32 ESP-IDF setup timing on an
empty bus), `i2cpico.yaml` with `probe.py TREE apply|revert` (RP2040/RP2350 levels right after `recover_()`, from a
temporary global; revert before committing) and `tab.py CAPTURE` (scenario versus the next boot's recovery result).

Cause found with it: the recovery sent exactly 9 SCL pulses and then checked SDA. A device that was receiving a write
counts the pulses as 1 bits and ACKs every ninth; when the 9th pulse is its ACK clock, SDA is low at the check, no
STOP follows, and every later boot lands in the next ACK again (stuck until a power cycle; one more pulse frees it).
Reads always recovered. The fix pulses only while SDA is low, as Linux, the ESP-IDF legacy driver and the ESP8266 twi
code do.

- Each boot, 6 s in, an interval lambda bit bangs the bus pins open drain: START, the address, then the register byte
  cut after k bits (write) or the register pointer, a repeated START and k read bits (read), stores the scenario in RTC
  user memory and calls `ESP.restart()`. RTC user memory survives resets and reflashes, so give each build its own
  `magic` or it never plants.
- `on_boot` priority 1100 logs SDA and SCL before the I2C setup, 999 the setup time, -100 the levels after all setups
  (and in the cycling mode frees a stuck SDA by hand, so every boot is a fresh trial).
- A 330 s capture of the cycling mode gives about 59 boots over the 18 scenarios. Before #22852 only "write after 8
  bits" failed (3 of 59) on a VL53L1X and on the Qwiic PIR; with it 0 of 59.
- `-s persist "true"` plants that write case once and never unsticks; later boots come from `serial_capture.py
  --reset`. Before #22852 7 of 7 boots stayed stuck (11 of 11 on the PIR), with it 0.
- The Qwiic PIR's ATtiny firmware may stretch SCL: the planting code waits for SCL to rise after releasing it.
- `vl53l1x` publishes metres: `id(x).state` prints 0 with `%.0f`; the readings are in the VERBOSE `'Distance' >>` line.
- A VL53L1X wedged this way makes `vl53l1x` fail setup with `Boot timeout` and the recovery log `SDA is held LOW after
  clock pulse cycle` on every reset. When testing a build without the fix, free the bus first (pulse SCL until SDA is
  high, then a STOP) and capture without `--reset`, so the upload reset lands on an idle bus.
- RP2040/RP2350 (arduino-pico 6.3.2): `digitalWrite(LOW)` on an `INPUT_PULLUP` pin switches it to output (open drain
  emulation), unlike ESP8266 where it only sets the latch. The recovery therefore pulled SDA low right before its own
  check and always reported `SDA is held LOW` there. LibreTiny does not use `i2c_bus_arduino.cpp` (software bus, no
  recovery).
- SparkFun Qwiic PIR (ATtiny84A firmware, 0x12, chip ID register 0x00 reads 0x72, 2.2k pull ups on the board): its
  debounce register 0x05 is little endian, but `qwiic_pir` wrote it big endian (`00 01` for 1 ms, so 256 ms on the
  device) until #22854. To fake a lost device without touching wires, `set_i2c_address(0x13)` on the qwiic_pir ids
  from an interval lambda for 15 s, then back: before #22854 every loop pass logged `Communication failed` (about 2400
  lines in 15 s with three instances), after it one warning per instance.
