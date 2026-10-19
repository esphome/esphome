# api

## Noise handshake and OTA encryption on the ESP8266 (#19062, #19233)

Host side timing is useless on a weak link; use an on-device bench with the minimum of five runs over
two boots. Provision a key with `noise_encryption_set_key` through aioesphomeapi and upload with
`espota2.run_ota(host, 8266, None, bin, noise_psk=key)` when the key is not in the yaml.

## Outgoing connection wake timing on a deep sleep build (#21077, 2026-10-18)

`templates/deep-sleep-outgoing-dut.yaml`: classic ESP32 ESP-IDF, `wifi: fast_connect`, `api: encryption` plus a
bare `outgoing_connection:`, `deep_sleep: run_duration: 15s, sleep_duration: 15s`, a `homeassistant` text sensor,
and `millis()` logged from `wifi: on_connect`, `api: on_client_connected` and the sensor's first `on_value`.
A local Home Assistant dev install runs the dial-in listener on 6054 (`lsof -nP -iTCP:6054`); reuse a device
name Home Assistant already has so its entry matches. The first wake after a flash gets an incoming HA connection and
`Remembered <ip> as the dial target`; every later wake logs `Dialing <ip>:6054`, and the target survives a reflash
of another build. Flash by USB, `serial_capture.py PORT 360 OUT --reset` for 12 cycles, then
`scripts/api_outgoing/cycles.py OUT LABEL` prints per cycle wifi/client/state millis and the client-wifi and
state-client gaps with min/median/max. On a classic devkit Wi-Fi takes 4.3 to 8 s after a wake, so compare the
client-wifi gap, not boot-to-state; run the PR twice around one base run (22 PR cycles vs 12 base) because the
spread is a few hundred ms. Results: https://github.com/esphome/esphome/pull/21077#issuecomment-6254098014

## Outgoing connection on a mains build after a Wi-Fi drop (#21085, 2026-10-18)

`templates/outgoing-wifi-drop-dut.yaml` (classic ESP32, `outgoing_connection: delay: 2s`, a "Wifi cycle" button doing
`wifi.disable`, 4 s, `wifi.enable`, a `debug` loop-time sensor, millis logged from the wifi and api triggers).
`scripts/api_outgoing/runcycles.sh LABEL BUILD_DIR N` flashes by USB, captures the console with `--ms` stamps and
presses the button N times 40 s apart; `wificycles.py CAPTURE LABEL` prints per drop the Wi-Fi-to-dial gap on
the capture clock, dials and dials closed, who connected first (dial or HA's incoming) and the loop-time maxima.
`runhost.sh LABEL BUILD_DIR` is the unreachable-host variant (`host: 192.0.2.1`, the connect pends 10 s) for the
loop cost of polling every loop. Lesson: Home Assistant refuses a dial-in while its own session state is READY
(`Refusing dial-in adoption`), so after a Wi-Fi drop every dial is closed and HA's incoming reconnect wins 11 to
33 s later on both builds; the measurable PR effect is the first dial landing in the loop the network flag flips.
`serial_capture.py --ms` was added for this. Results: https://github.com/esphome/esphome/pull/21085#issuecomment-6254742791
