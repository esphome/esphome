# Fake I2C devices without a chip

## Fake devices on an in-firmware I2C bus

When there is no chip and no wiring, subclass `i2c::I2CBus` in an include, implement `write_readv()` only, answer
the registers the driver reads and check each write. Point the component at it from an `on_boot` lambda that runs
before the component's `setup()`: `id(x)->set_i2c_bus(&fake::bus)` at priority 950, which is ahead of every I2C
component (hubs set up at IO, 900, sensors at DATA, 600). The configs here that use 700 to 1000 work for their
components; use 950 for a new one. Keep a real `i2c:` block in the config so validation passes. Bulk
loops in a button lambda must call `App.feed_wdt()` and keep their per-iteration logs below INFO: 2000 DEBUG lines at
115200 baud trip the task watchdog, which is a fault of the harness and not of the PR.

`templates/i2c_fake/` holds the fakes; each `*-common.yaml` or `*-test.yaml` is a classic ESP32 or ESP32-S3 config
that includes its `.hpp` (add `esphome: name:` for the `-common` ones):
- `ccsfake.hpp`, `ccs-common.yaml`: CCS811 at 0x5A (#21927). Answers setup and update reads and checks every
  ENV_DATA write against the humidity and temperature the case set; "CCS Cases" runs edge values, "CCS Sweep" 2000
  random ones and logs `SWEEP checked N bad M`.
- `eefake.hpp`, `ee-common.yaml`: HTE501 (0x40) and TEE501 (0x48) with CRC words; "Next" steps through raw
  temperature words and logs the value a signed hundredths decode should give (#21930).
- `scdfake.hpp`, `scd-common.yaml`: SCD4x at 0x62 and 0x63 with Sensirion CRC; logs every `0xE000` ambient pressure
  write. The second instance has a fixed `ambient_pressure_compensation` to cover the setup path (#21935).
- `sxfake.hpp`, `sx-common.yaml`: two SX1509 register files with reset values and the `7D 12/34` reset, recording
  every write. Compare per address: hub setup order can legitimately move between hubs (#21897). Outputs of an Apply
  platform are distinct template types, so a lambda list of them needs
  `std::initializer_list<output::FloatOutput *>{...}`.
- `msfake.hpp`: eight MS8607 pressure/temperature fakes at 0x76 and up and humidity fakes at 0x40 and up, with
  faults per device (reset NACKs, bad PROM CRC4, zero D1 every Nth read, humidity CRC8 errors, out of range D2, full
  scale humidity). It counts every command, so skipped conversions show in `report()`. Point both the component and
  its `humidity_i2c_id` device at the bus. That driver logs no values, so give each sensor an INFO `on_value` log
  (#16487).
- `htfake.hpp`, `ht16k33-char-test.yaml`: HT16K33 chips at 0x70 to 0x77 (0x74 and 0x76 NACK) keeping the 16 byte
  display RAM and a command trace, rendered back as text for the Adafruit 0.56" 7-segment backpack ("12:34",
  decimal points, "!" for stray bytes). The test runs 64 checks (setup order, font, colon and dots, chained chips,
  printf/strftime/clock, dimming, blink, standby, NACK) and ends with `ALL DONE pass=N fail=M` (#8520). It found a
  crash (null glyph table) when one display had `add_characters` and another did not.
- `ssfake.hpp`, `seesaw-test.yaml`: Adafruit seesaw at 0x36 (ATtiny817), 0x49 (SAMD09) and 0x4A (NACK): status,
  GPIO bulk with the pull latch, ADC, touch, encoder and NeoPixel with chunk offsets, and the write to read gap per
  module. A 100 ms interval step machine drives it and ends with `ALL DONE` (#13538).
- `bldcfake.hpp` (three M5Unit-BLDC units at 0x65 to 0x67 with simple motor physics, one that never spins down),
  `gtfake.hpp` (GT911 at 0x5D: switches, max values, touch points and key byte) and `sdpfake.hpp` (SDP3x/SDP8xx at
  0x21 to 0x26: product IDs, an unknown product, a failing read every third time, a failing ID read, stop refused
  while idle) have no config yet; wire them the same way.

Check the PR head again just before posting results: a head that moves during a run means a retest.

## Pico 2 W as an I2C target (#22236)

`scripts/pico_i2c_target/`: `picofake.hpp` is a raw RP2350 I2C0 interrupt handler on GP4 (SDA) and GP5 (SCL) at 0x76 that
answers one byte per read request from a 256 byte register file, so any read length is byte exact. `picofake.yaml`
plays a BME280 (`chip_id` 0x60) or BMP280 (0x58) with the datasheet calibration; every 15 s it switches between the
datasheet example readings and the 0x80000 "no measurement" values, and logs `STAT` counters every 5 s. The DUT is
an ESP32-C3 (`dut-common.yaml`, GPIO6/GPIO7) with two sensors on the one fake (`dut-bme280.yaml`,
`dut-bmp280.yaml`), the second with oversampling NONE. `run.sh bme280|bmp280 LABEL` flashes the DUT, captures both
boards and prints the distinct values per sensor; diff the output of dev and the PR.

The ESP32-S3 I2C slave v2 driver cannot emulate a register device: answering from a task and straight from the ISR
both gave shifted multi-byte reads. Use the Pico.

## DS2482-800 simulator (ds248x)

`scripts/ds248x_sim/`: `dssim.yaml` with `sim.hpp` turns a Pico 2 W into a DS2482-800 at 0x18 (GP4 SDA, GP5 SCL)
with four DS18B20s: channel 0 at 21.5 °C, channel 1 at 33.0 °C, channel 2 at -5.25 and 40.0 °C. The DUT is an
ESP32-C3 (`dspd.yaml`, GPIO4/GPIO5 plus GND); the internal pull-ups are enough at 50 kHz. Faults are one character
on the Pico's USB console: `c` NACKs the read pointer write after the next channel select, `z` NACKs the 9 reads
after the next 0xBE, `b` and `k` stick busy with a failed reset or config write, `x` and `X` make it dead and alive.
`SC=<serial_capture.py> python3 drive.py TAG SIM_PORT DUT_PORT [c,z,b,k,dead]` resets the DUT per phase and prints
readings, wrong values, log counts and the simulator's counters (`wrong_ch` counts ROM matches on the wrong
channel). A phase fails when any of the four sensors got no readings, a temperature is wrong, the DUT capture
failed (stderr in `TAG-<phase>.err`), a fault never moved its injection counter or the simulator never logged
`DEAD` for the `dead` phase.

- Sensors polled at the same interval go through the channels in blocks, so a stale channel cache never shows.
  Poll them at different intervals, as `dspd.yaml` does (1, 1.3, 1.7 and 2.3 s).
- The NACKs use the RP2350 `slv_data_nack_only` register, set at STOP.

## I2C fault injection in software (vl53l0x)

`scripts/i2c_fault/`: `faultbus.h` wraps the real `I2CBus` and returns NACK for chosen transfers to one address. An
`on_boot` priority 800 lambda swaps the sensor's bus (`id(d)->set_i2c_bus(&fb)`) before its setup, and an interval
arms the faults. `vlpf.yaml` runs a VL53L0X on an ESP32-S3 (GPIO8/GPIO9) through quiet, fault A (the next
measurement-in-progress read fails), quiet, fault B (the next measurement start fails) and quiet phases over 150 s,
with a model ID probe every 10 s to tell a driver stall from a dead chip. `run.sh LABEL` builds, flashes, captures
and runs `an.py`, which counts readings per `PHASE` line. It fails when the phases are not quiet, faultA, quiet,
faultB, quiet (a short capture or a restart), the first quiet phase has no readings, a later quiet phase reads less
than half as often as the first, anything crashed or rebooted, or a fault never fired.

Prefer this to glitching the wire. Holding SDA low by rerouting the pin (`esp_rom_gpio_connect_out_signal` to
`SIG_GPIO_OUT_IDX` and back to `I2CEXT0_SDA_OUT_IDX`) wedges the ESP-IDF controller, which then needs
`i2c_master_bus_reset()` after each glitch, and glitches timed onto writes left the VL53L0X answering 0x00 until a
power cycle.
