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
  random ones and logs `SWEEP checked N bad M bad_reads R bad_writes W`. A read without a register, of a register
  the fake does not model or of the wrong length gets a NACK and counts in `bad_reads`; a write other than the
  driver's SW_RESET, APP_START, MEAS_MODE and BASELINE writes (each with its own length) gets a NACK and counts in
  `bad_writes`. The CASES and SWEEP lines end in PASS only when something was checked and `bad`, `bad_reads` and
  `bad_writes` stayed 0, and in FAIL otherwise.
- `eefake.hpp`, `ee-common.yaml`: HTE501 (0x40) and TEE501 (0x48) with CRC words; "Next" steps through raw
  temperature words and logs the value a signed hundredths decode should give (#21930). A command other than
  identification and measure, and a read longer than the reply, gets a NACK and counts as `bad`. The DONE line
  200 ms later, after both reads, shows the running `bad` total, the measure replies each driver read and the
  published values; it ends in PASS only while `bad` is 0, both drivers read a reply this case and both published
  the expected temperature (and HTE501 the fixed humidity).
- `scdfake.hpp`, `scd-common.yaml`: SCD4x at 0x62 and 0x63 with Sensirion CRC; logs every `0xE000` ambient pressure
  write, and the CASE and SHOW lines count writes with a bad CRC apart from the good ones. A command the scd4x driver
  does not send, one sent with the wrong length (2 bytes bare, 5 bytes with one CRC word), or any other command with a
  bad CRC, gets a NACK and counts as `bad writes`. A read after a command the fake does not answer, or one longer than the command's
  reply, gets a NACK and counts as `unknown reads`. The second instance has a fixed `ambient_pressure_compensation`
  to cover the setup path (#21935). Each CASE line ends in PASS only when bad CRC, bad writes and unknown reads did
  not move, the `0xE000` writes match `expect_e000()` (none, or one with the expected value) and, for a source case,
  CO2 was published. `RANGE_CHECKED` in `scdfake.hpp` picks the rule: false truncates as dev does, true skips values
  outside 700 to 1200 hPa and rounds as #21935 did. The SHOW line ends in PASS when nothing bad was counted, neither
  instance failed and 0x63 got exactly one write of its fixed value.
- `sxfake.hpp`, `sx-common.yaml`: two SX1509 register files with reset values and the `7D 12/34` reset, recording
  every write. Only a byte that lands on RegReset (0x7D) drives the reset, so a burst through it keeps the bytes for
  0x7E and 0x7F. A read without a register, or a transfer past 0x7F, gets a NACK and `dump_log()` warns with the
  count. Compare per address: hub setup order can legitimately move between hubs (#21897). Outputs of an Apply
  platform are distinct template types, so a lambda list of them needs
  `std::initializer_list<output::FloatOutput *>{...}`. Each case ends with an END line, PASS only while no write was
  dropped and no transfer was bad; the writes themselves are judged by the diff.
- `msfake.hpp`: eight MS8607 pressure/temperature fakes at 0x76 and up and humidity fakes at 0x40 and up, with faults
  per device (reset NACKs, bad PROM CRC4, zero D1 every Nth read, humidity CRC8 errors, out of range D2, full scale
  humidity). It counts every command, so skipped conversions show in `report()`, and an ADC read with no conversion
  pending answers 0 like the real chip and counts as `adc_no_conv`, and a humidity read with no measure pending gets
  a NACK like the real chip in no-hold mode and counts as `read_no_measure` (a reset aborts either); each injected
  fault counts when it is delivered (`inj_reset_nack` with `reset_nack_left`, `inj_zero_d1`, `inj_bad_crc`), so a
  run that never reached a fault path shows 0 there; a PROM read, conversion or humidity measure
  before that device's first reset counts as `no_reset`; a humidity read of any shape but a bare
  3 byte read, and a pressure/temperature read that is not a 2 byte read of a PROM word (even addresses 0xA0 to 0xAC) or
  a 3 byte ADC read, gets a NACK and counts as `bad_reads`; a bare read answers the last PROM or ADC pointer and never
  repeats a reset or conversion; a command write the fake does not know, or one longer than a
  byte, gets a NACK and counts as `bad_cmds`. Point both the component and its `humidity_i2c_id` device at the bus. That
  driver logs no values, so give each sensor an INFO `on_value` log (#16487).
- `htfake.hpp`, `ht16k33-char-test.yaml`: HT16K33 chips at 0x70 to 0x77 (0x74 and 0x76 NACK) keeping the 16 byte
  display RAM and a command trace, rendered back as text for the Adafruit 0.56" 7-segment backpack ("12:34",
  decimal points, "!" for stray bytes); a read gets a NACK and counts as malformed. The test runs 64 checks (setup order, font, colon and dots, chained chips,
  printf/strftime/clock, dimming, blink, standby, NACK) and ends with `ALL DONE pass=N fail=M` and PASS or FAIL (#8520). It found a
  crash (null glyph table) when one display had `add_characters` and another did not.
- `ssfake.hpp`, `seesaw-test.yaml`: Adafruit seesaw at 0x36 (ATtiny817), 0x49 (SAMD09) and 0x4A (NACK): status,
  GPIO bulk with the pull latch, ADC, touch, encoder and NeoPixel with chunk offsets, and the write to read gap per
  module. A read it does not model, a write without a register and a write to a module or register it does not model
  (or with the wrong data length) get a NACK and count in `bad`. A 100 ms interval step machine drives it and ends with `ALL DONE` and PASS or FAIL (#13538).
- `bldcfake.hpp` (three M5Unit-BLDC units at 0x65 to 0x67 with simple motor physics, one that never spins down),
  `gtfake.hpp` (GT911 at 0x5D: switches, max values, touch points and key byte) and `sdpfake.hpp` (SDP3x/SDP8xx at
  0x21 to 0x26: product IDs, an unknown product, a failing read every third time, a failing ID read, stop refused
  while idle) have no config yet; wire them the same way. Each fake sets up its own faults in its constructor, so
  there is no init step to call. Each NACKs a read it does not model or one of the wrong
  length, and a write or command it does not know, and counts them as `bad_reads` and `bad_writes` (`bad_cmds` for
  the SDP) in `report()`; check those are 0. The SDP report also counts each injected ID read NACK at 0x24 as
  `id_fail`, so 0 there means the driver never reached that fault.

Check the PR head again just before posting results: a head that moves during a run means a retest.

## Pico 2 W as an I2C target (#22236)

`scripts/pico_i2c_target/`: `picofake.hpp` is a raw RP2350 I2C0 interrupt handler on GP4 (SDA) and GP5 (SCL) at 0x76 that
answers one byte per read request from a 256 byte register file, so any read length is byte exact. `picofake.yaml`
plays a BME280 (`chip_id` 0x60) or BMP280 (0x58) with the datasheet calibration; every 15 s it switches between the
datasheet example readings and the 0x80000 "no measurement" values, and logs `STAT` counters every 5 s. `bad`
counts reads and writes of registers the drivers do not use (soft reset only with 0xB6, read pointer wrapping past
0xFF). The DUT is
an ESP32-C3 (`dut-common.yaml`, GPIO6/GPIO7) with two sensors on the one fake (`dut-bme280.yaml`,
`dut-bmp280.yaml`), the second with oversampling NONE. `run.sh bme280|bmp280 LABEL` flashes the DUT, starts the
fake capture and waits for a `STAT` line as the baseline, then resets and captures the DUT and prints the distinct
values per sensor. The fake capture runs on until a `STAT` line arrives after the DUT capture ends, so that line
covers the whole run. It prints PASS when every sensor gave a numeric state, that `STAT` line came, and the fake's
`abrt` (read replies the controller aborted) and `bad` counts did not move from the baseline; diff the output of dev
and the PR.

The ESP32-S3 I2C slave v2 driver cannot emulate a register device: answering from a task and straight from the ISR
both gave shifted multi-byte reads. Use the Pico.

## DS2482-800 simulator (ds248x)

`scripts/ds248x_sim/`: `dssim.yaml` with `sim.hpp` turns a Pico 2 W into a DS2482-800 at 0x18 (GP4 SDA, GP5 SCL)
with four DS18B20s: channel 0 at 21.5 °C, channel 1 at 33.0 °C, channel 2 at -5.25 and 40.0 °C. The DUT is an
ESP32-C3 (`dspd.yaml`, GPIO4/GPIO5 plus GND); the internal pull-ups are enough at 50 kHz. Faults are one character
on the Pico's USB console: `c` NACKs the read pointer write after the next channel select, `z` NACKs the 9 reads
after the next 0xBE, `b` and `k` stick busy with a failed reset or config write, `x` and `X` make it dead and alive.
`SC=<serial_capture.py> python3 drive.py TAG SIM_PORT DUT_PORT [c,cd,z,b,k,dead]` (default `c,z,b,k,dead`; `cd`
runs the `c` fault densely, 28 times about 3 s apart) resets the DUT per phase and prints
readings, wrong values, log counts and the simulator's counters (`wrong_ch` counts ROM matches on the wrong
channel). The DUT is captured with `--ms` so the log can be split at the last fault: the recovery window starts
`SETTLE` (3) seconds after it. A phase fails when any of the four sensors got no correct reading in the recovery
window, the DUT logged a CRC, held low, busy timeout or no response line in it, a temperature is wrong, a phase other
than `dead` logged `recovery failed`, `DS248x hub failed` or a component marked failed, a ds248x warning is still
set at the end of the phase, the DUT capture
failed (stderr in `TAG-<phase>.err`), a fault moved its injection counter (counted when the DUT meets the
fault, not when it is armed) by other than the number of faults sent (`arm_lost` counts arms that landed while one
of the same fault was still pending and merged into it), a NACK fault never moved `nacked`, a write delivered data while a NACK was armed
(`nack_miss`, also counted when the write is followed by a repeated start into a read; the NACK never reached the wire and stays pending), the DUT sent an address-only write (`empty_wr`), a write was cut off by a repeated start into another write and
never processed (`dropped`),
a fault is still pending (`pend=1`) at the end of the
phase, the simulator counted a config write with a wrong complement (`cfg_bad`) or an unknown, short or overlong
command (`bad_cmd`), a counter is missing from the simulator's CNT line, the simulator aborted a read reply
(`tx_abrt`, not gated in the `dead` phase, which switches the controller off), a phase other than `b` and `k` (which
reset the chip under the driver) counted `wrong_ch` or a 1-Wire command without the active pull-up bit (`ops_no_apu`),
the simulator never logged `DEAD` for the `dead`
phase, or the DUT log of the `dead` phase has no sign of the outage (`did not respond`, a ds248x warning,
`DS248x hub failed` or a component marked failed). It ends with PASS or `FAIL:` and the failed phases. The whole
phase CRC, held low, busy timeout and no response counts are printed for information: the faults cause them.

- Sensors polled at the same interval go through the channels in blocks, so a stale channel cache never shows.
  Poll them at different intervals, as `dspd.yaml` does (1, 1.3, 1.7 and 2.3 s).
- The NACKs use the RP2350 `slv_data_nack_only` register, set at STOP.
- A NACKed write and an address-only write both end with no data, so the simulator cannot tell them apart.
  The ds248x driver never sends one, `dspd.yaml` sets `scan: false` and the simulator only sees STOPs after its
  own address, so `empty_wr` (address-only writes outside a NACK window) stays 0; a nonzero count means `nacked`
  may include writes that never carried a NACKed byte.

## I2C fault injection in software (vl53l0x)

`scripts/i2c_fault/`: `faultbus.h` wraps the real `I2CBus` and returns NACK for chosen transfers to one address. An
`on_boot` priority 800 lambda swaps the sensor's bus (`id(d)->set_i2c_bus(&fb)`) before its setup, and an interval
arms the faults. `vlpf.yaml` runs a VL53L0X on an ESP32-S3 (GPIO8/GPIO9) through quiet, fault A (the next
measurement-in-progress read fails), quiet, fault B (the next measurement start fails) and quiet phases over 150 s,
with a model ID probe every 10 s to tell a driver stall from a dead chip. `run.sh LABEL` builds, flashes, captures
and runs `an.py`, which counts readings per `PHASE` line. Each `PHASE` line clears a fault that was armed and has
not fired yet and logs the fired totals so far, so `an.py` counts the fires of each phase. It fails when the phases are not quiet, faultA, quiet,
faultB, quiet (a short capture or a restart), the first quiet phase has no readings, a later quiet phase reads less
than half as often as the first, a later quiet phase reads NaN (`nan` or `-nan`) or logs the driver's "update called before prior
reading" warning when the first did not (a target out of range reads NaN in every phase), a quiet phase has a failed
model ID probe, one that did not return 0xEE or no successful probe at all,
anything crashed or rebooted, or a fault did not fire in its own phase or fired in another one.

Prefer this to glitching the wire. Holding SDA low by rerouting the pin (`esp_rom_gpio_connect_out_signal` to
`SIG_GPIO_OUT_IDX` and back to `I2CEXT0_SDA_OUT_IDX`) wedges the ESP-IDF controller, which then needs
`i2c_master_bus_reset()` after each glitch, and glitches timed onto writes left the VL53L0X answering 0x00 until a
power cycle.
