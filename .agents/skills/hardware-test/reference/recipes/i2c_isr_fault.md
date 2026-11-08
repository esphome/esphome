# I2C master interrupt storm and stuck bus faults (esphome#22270 / #22396, 2026-11-06 to 11-07)

`scripts/i2c_isr_fault/inasim.hpp` is a software INA219 at 0x40 answered on the ESP32 master's own pads (SDA GPIO32,
SCL GPIO33, nothing wired): the slave pulls a line low by switching the pad's output mux from the I2C0 signal to plain
GPIO output 0 (a wired AND with the master), from a GPIO ISR on core 0 that spins through one transaction. A core 0
task re-asserts long stuck lines every tick. From 20 s after boot it injects seeded faults into `p_fault` per mille of
transactions (garbage bits, NACK, clock stretch, SCL/SDA held low up to `hold_max_ms`, glitches, vanish).

`i2cbase.yaml` (package): `i2c:` at `freq` with `scan: true`, an `ina219` polled every `ina_interval`, a bus voltage
register read every `hammer`, a boot self test, and 30 s `MASTER`/`SLAVE` counter lines (ok, timeouts by kind,
garbage, longest failed run, loop stalls). `example.yaml` shows the substitutions: framework and version (`fw`,
`fwver`), `seed` (seed 1 at 5 % hangs dev with an interrupt watchdog on CPU0 at about 245 s, the #22270 report),
`p_fault`, `fault_mask`. `run.sh LABEL [SECONDS]` builds, flashes and captures (640 s default); `summary.sh` prints
crashes, watchdogs, timeout kinds and the minimum ok reads per window.

Findings: ESP-IDF 5.4.2 has a different failure (the driver waits for bus idle with no timeout after a NACK, so the task
watchdog reboots in a loop with or without the guard); 5.5.5, 5.5.6 and 6.0.1 hang on the storm without the guard and
run clean with it. ESPHome never uses the driver's async mode, so there is no asynchronous path to test.

Results: https://github.com/esphome/esphome/pull/22396#issuecomment-6713396173
