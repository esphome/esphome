# Sensor, binary_sensor and text_sensor filter A/B (#21831, #21813, #21830)

`scripts/sensor_filters` needs only a board. `sensors.yaml` holds 29 template sensors with every offset, multiply,
clamp, NTC, calibrate_linear, delta and lambda filter shape, plus a sensor with id `x` (a value lambda that reads an
id named `x`). `sf.yaml` adds binary and text sensor lambda filters and an interval that feeds the 50 inputs of
`sf.hpp` (NaN, ±inf, -0.0, values on and across the bounds) 200 ms apart, logging one line per input with every
filter output as float bits (`-` means dropped), then `DONE`.

- `run.sh esp8266|bk72xx LABEL` writes the per-run file, builds, flashes, captures and writes `out-sf<kind><LABEL>.out`
  (100 lines). Compare a PR build with dev using `cmp`: identical files mean identical filter output, including
  which values were dropped. ESP8266 matters because its float math is in software.
- The template sensor ids are `flt1`, `flt2` ...: `s8` and `s16` clash with ESP8266 SDK typedefs.
- Grep the build's `main.cpp` for the new code (for example `clamp_filter_value`, `delta_passes`, generated filter
  functions) to prove the build came from the PR tree.
- `BASELINE` sets the delta baseline: a lambda works everywhere, a plain number crashed before #21811.
- Between boards the only difference is the sign of the NaN that `to_ntc_temperature` returns for 0 Ω (libm), on dev
  too, so compare builds on the same board.
- The ESP8266 esptool stub can hit serial noise on a CH340 and the BK7238 link sometimes times out once; the runner
  retries.
- Results: https://github.com/esphome/esphome/pull/21831#issuecomment-6490999545
