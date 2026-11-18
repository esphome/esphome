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
