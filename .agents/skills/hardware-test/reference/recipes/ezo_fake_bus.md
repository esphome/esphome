# EZO circuits without hardware: a fake I2C bus (#21840)

`scripts/ezo_fake_bus`: the real `ezo` component runs on a classic ESP32, but an `on_boot` priority 900 lambda calls
`id(ph).set_i2c_bus(&fakeezo::bus)`. `fake_ezo.hpp` is an `i2c::I2CBus` subclass that answers like an EZO pH circuit
and logs every write and read with the ms since the command: `R`, `i`, `Slope,?`, `Cal,?`, `T,?`, `L,?`, `Name,?`,
and test replies `pend` (254 twice), `bad` (2), `nodata` (255), `nak` (read NACK) and `full31` (32 bytes with no
terminator), and `sNN` (an NN-character reply, terminated when the read has room). `ezo.yaml` runs the steps:
command bursts larger than the queue, custom commands, calibration, LED, sleep and address changes, and the error
replies.

- The same trick works for any I2C device whose driver only needs write and read: swap the bus pointer, no wiring.
  A real `i2c:` block is still needed so the config validates.
- `run.sh LABEL` builds, flashes, captures 90 s and writes `out-ezo<LABEL>.events`. Compare dev and the PR: the
  replies, callback texts, read values and the wait before each read (912 ms for reads and calibration, 304 ms for
  the rest) must match; differences in queue limits are the PR's intended change.
- Call plain `millis()` in a fake: Arduino builds define a `millis()` macro (`esphome::millis()`), so
  `esphome::millis()` breaks there, and on ESP-IDF the plain name resolves through `using namespace esphome`.
- Found with it: dev read past its 32-byte buffer on an unterminated reply (a custom callback got 56 bytes), and
  two queued address changes both went to the first new address.
- Results: https://github.com/esphome/esphome/pull/21840#issuecomment-6493565259

## Trigger argument lifetime (StringRef or an owning copy, #22204)

Give each text trigger two automations (`- then:` list items): a synchronous one that logs `x`, `x.size()` and
`strlen(x.c_str())` and hands `x` to a parallel `script` (`text: !lambda return x;`, the script delays 2.5 s), and
one with `delay: 3s` or `wait_until` that logs `x` afterwards. Fire bursts so newer replies reuse the reply buffer
during the wait, then diff the synchronous texts, in order, against the delayed and script texts. Check the generated
`main.cpp` for `StringRef` or `TriggerForwarder<std::string>`. The fake's `sNN` replies give texts of any length.

## A real EZO circuit

An EZO-HUM ships in UART mode (blinking green, streams `33.91\r` every second at 9600 baud). To switch it to I2C
without bridging PGND, flash a one-shot software UART build (TX to the EZO RX, RX from the EZO TX) that writes
`I2C,111\r`; the LED turns blue and the circuit answers at 0x6F. Replies: `i` gives `HUM,1.01`, `Status`
`?STATUS,S,3.31`, `O,?` `?O,HUM,T,DEW`, and `R` with all outputs `34.82,22.03,Dew,5.85`; `Name,x` returns no text.
EZO-HUM rejects `T,?`, `Slope,?` and `Cal,?` (syntax error), so `on_t`, `on_slope` and `on_calibration` need another
circuit or the fake bus. Log every callback with an `EV` prefix and count them per step.
