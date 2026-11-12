# modem

## PPPoS modem component with no modem attached (#6721)

A boot check on a classic ESP32 devkit with the modem UART pins left open. `templates/modem/modem-common.yaml` is
the package (`api:`, debug `loop_time` and `free` sensors, uptime, buttons for `modem.enable`, `modem.disable` and
`modem.send_at`, a 20 s alive log); `modemo.yaml` is the modem only build with `reboot_timeout: 2min` and the
component test config's `on_boot` block at `priority: -100` (earlier, `modem.send_at` runs before the modem's
`setup()` and logs `Modem DCE not ready`); `modemw.yaml` adds `wifi:` with `reboot_timeout: 0s` so the API stays
reachable. Expect `Modem not responding at 115200 baud` once per pass with each pass blocking the loop about 1 s,
the reboot at the configured timeout (`SW_CPU_RESET`), and a `modem.disable` with no modem answering to block
the loop about 37 s (`Failed to leave data mode`, then the radio off timeout) without tripping the 30 s watchdog
floor because the component feeds it in between. Drive the Wi-Fi build with
`scripts/modem/mdrive.py HOST LOGFILE list press:NAME wait:S` (plaintext API; prints sensor and text sensor
states, saves the device log). Before 8cc8df4a1f a modem only config with `api:` failed validation because a
config dependent `AUTO_LOAD` added `network` after the dependency check; an explicit `network:` block was the
workaround. Results: https://github.com/esphome/esphome/pull/6721#issuecomment-6347395159 and
https://github.com/esphome/esphome/pull/6721#issuecomment-6348520416

## SIM7080G on an M5Stack Atom Lite (#6721)

The M5 Unit CatM (SIM7080G) on the Atom Lite's Grove port: RX GPIO32, TX GPIO26, `model: SIM7080`.
`templates/modem/modem-passthrough.yaml` is a raw UART passthrough with both directions logged, and
`scripts/modem/mdrive.py HOST LOGFILE text:AT+CPIN? wait:5 ...` sends AT commands through it and saves the
replies, to check what the module does with no component in the way.

- With no SIM, the component sets the `SIM not ready` warning and polls `AT+CPIN?` once a second.
- With a SIM that has no data plan, `AT+CGATT=1` blocks the AT channel for about 141 s and then answers `ERROR`.
  The module answers nothing meanwhile, so the component logs `Network attachment request refused` and then `Modem not
  responding`, and loops every 2 minutes 22 seconds.
- `AT+CFUN=1,1` leaves the module silent for about 2.5 minutes. It comes back without a power cycle.
- A module left in CMUX (`AT+CMUX=0`, then an ESP restart) recovered 10 of 10 times (`Autodetect mode: 5`).
- PPP and a warm reboot during PPP need a SIM with a working data plan.
