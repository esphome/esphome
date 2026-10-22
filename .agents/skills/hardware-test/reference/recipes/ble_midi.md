# ble_midi

## BLE MIDI client against a fake instrument (#18508)

No MIDI hardware needed: `templates/ble_midi/midiperi.yaml` turns a second ESP32 (an S3 with ethernet, so the
API stays reachable while BLE is busy) into a BLE MIDI peripheral with `esp32_ble_server`: the MIDI service
advertised, the MIDI characteristic with read, write, write without response and notify, an `on_write` that logs
the bytes, buttons that notify hand built packets (channel messages, two messages in one packet, running status,
SysEx in one packet, split over two, oversize, cut short, clock, aftertouch, orphan data bytes, a 50 packet burst)
and a restart button for the disconnect test. `midicli.yaml` is the component test config on a classic devkit with
`wifi:`, `api:`, the peripheral's BLE address in `PERI_MAC` (the base MAC plus 2, printed by `esp32_ble` in
`dump_config`), `on_sysex` and `on_message` lambdas that print the bytes, debug sensors and out of range send
buttons. Drive both over the plaintext API with `scripts/modem/mdrive.py HOST LOGFILE press:NAME wait:S` 3 s
apart and compare the peripheral's `TX` and `RX` lines with the client's decoded lines. Gotcha:
`ble_server.characteristic.set_value` latches the value of its first play and re-applies it before every notify,
so the peripheral script sets the value from a lambda (`id(midi_io).set_value(std::vector<uint8_t>(pkt));
id(midi_io).notify();`). At VERBOSE the client logs four lines per message, so a burst shows an
`esp32_ble took a long time` of a couple of seconds that is the serial port, not the parser.
Results: https://github.com/esphome/esphome/pull/18508#issuecomment-6348272717
