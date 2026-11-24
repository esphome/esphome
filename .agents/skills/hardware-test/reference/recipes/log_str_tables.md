# Log string tables

## Name table A/B without chips (#22855, #22856, #22857, #22860)

Moving the names a component logs into PROGMEM string tables must not change one byte of the log. Build one image
per board for the PR head and one for its merge base, play the same script on both and diff the serial logs.
`scripts/log_str_tables/` has one package per PR: `l1-common.yaml` (#22855), `l2-common.yaml` (#22856),
`l3-common.yaml` (#22857, plus `l3-va.yaml` for the voice_assistant part on an ESP32) and `l4-common.yaml` (#22860,
entity names over the API). Each runs a scripted `on_boot` sequence that logs `MARK <step>` lines and ends with
`MARK ALLDONE`. The per-board file sets the substitutions the package uses (`name`, `sda`, `scl`, `uart_tx`,
`uart_rx`, and for some `step_pin`, `dir_pin`, `ir_pin`, `st_a` to `st_d`, `uart_share`) and the platform block.
Name the builds `l<PR><p|b><board>` (for example `l1p66` and `l1b66`), which `norm.py` and `cover.py` expect.

- `fakes.hpp` (via `esphome: includes:`): `fk::bus` I2C register file (ds3231 at 0x68, ens210 at 0x43; swap it in
  with `set_i2c_bus` from an `on_boot` priority 1100), `fk::tmc` TMC22xx single-wire UART (echo, CRC replies; set
  `regs[0x01]`/`regs[0x6F]` to raise GSTAT and DRV_STATUS events), `fk::sen` SEN0609 console (echoes each command,
  `Done` to stop, start, save and set, silent on setLedMode, resetCfg and get*), `fk::cn` CN105 heat pump (ignores
  the first connect, answers 0x5A, 0x42 and 0x41), `fk::ens_set()` ENS210 frames with a good or bad CRC7 and valid
  bit. Swap a UART device with `id(x)->set_uart_parent(&fk::tmc)`.
- `vadrive.py HOST`: plays Home Assistant for voice_assistant (button "VA Start", API audio, VAD, STT, intent, TTS
  stream with audio, run end) and reaches every state the component sets. `handle_audio` gets two arguments.
- `norm.py CAPTURE [NAME_RE]` strips timestamps, line numbers, the device name, the compile time and timing values,
  and cuts at `MARK ALLDONE`; it exits nonzero when the marker is missing. `fdiff.sh A.norm B.norm` diffs without
  radio, heap and hash lines; add `--sorted` to compare the sorted line sets when independent timers interleave.
  `cover.py PR BOARD` (groups 1 to 3) counts which table names printed, per component, and whether PR and base
  lines match.
- Restored entity state (fan speed and preset, cover position) carries across flashes: run PR, base, then PR again
  and compare the two PR runs too.
- One-pin IR loopback works on both boards (ESP8266 GPIO13 with an interval `pinMode(13, OUTPUT)`, ESP32 GPIO27).
- On the ESP8266 the tables saved flash only; the RAM sections stayed identical because `LOG_STR` already kept the
  strings in flash.
