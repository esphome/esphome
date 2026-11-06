# EZO circuits without hardware: a fake I2C bus (#21840)

`scripts/ezo_fake_bus`: the real `ezo` component runs on a classic ESP32, but an `on_boot` priority 900 lambda calls
`id(ph).set_i2c_bus(&fakeezo::bus)`. `fake_ezo.hpp` is an `i2c::I2CBus` subclass that answers like an EZO pH circuit
and logs every write and read with the ms since the command: `R`, `i`, `Slope,?`, `Cal,?`, `T,?`, `L,?`, `Name,?`,
and test replies `pend` (254 twice), `bad` (2), `nodata` (255), `nak` (read NACK) and `full31` (32 bytes with no
terminator). `ezo.yaml` runs the steps: command bursts larger than the queue, custom commands, calibration, LED,
sleep and address changes, and the error replies.

- The same trick works for any I2C device whose driver only needs write and read: swap the bus pointer, no wiring.
  A real `i2c:` block is still needed so the config validates.
- `run.sh LABEL` builds, flashes, captures 90 s and writes `out-ezo<LABEL>.events`. Compare dev and the PR: the
  replies, callback texts, read values and the wait before each read (912 ms for reads and calibration, 304 ms for
  the rest) must match; differences in queue limits are the PR's intended change.
- Found with it: dev read past its 32-byte buffer on an unterminated reply (a custom callback got 56 bytes), and
  two queued address changes both went to the first new address.
- Results: https://github.com/esphome/esphome/pull/21840#issuecomment-6493565259
