# api

## Noise handshake and OTA encryption on the ESP8266 (#19062, #19233)

Host side timing is useless on a weak link; use an on-device bench with the minimum of five runs over
two boots. Provision a key with `noise_encryption_set_key` through aioesphomeapi and upload with
`espota2.run_ota(host, 8266, None, bin, noise_psk=key)` when the key is not in the yaml.

## Outgoing connection wake timing on a deep sleep build (#21077)

`templates/deep-sleep-outgoing-dut.yaml`: classic ESP32 ESP-IDF, `wifi: fast_connect`, `api: encryption` with a bare
`outgoing_connection:`, `deep_sleep` 15 s run and 15 s sleep, a `homeassistant` text sensor, and `millis()` logged
from `wifi: on_connect`, `api: on_client_connected` and the sensor's first `on_value`. Home Assistant's dial-in
listener is on 6054; reuse a device name it already has. Flash by USB, `serial_capture.py PORT 360 OUT --reset` for
12 cycles, then `scripts/api_outgoing/cycles.py OUT LABEL` prints per cycle the wifi, client and state millis with
the client-wifi and state-client gaps. Wi-Fi takes 4 to 8 s after a wake, so compare the client-wifi gap, and run
the PR twice around one base run because the spread is a few hundred ms.
Results: https://github.com/esphome/esphome/pull/21077#issuecomment-6254098014

## Outgoing connection on a mains build after a Wi-Fi drop (#21085)

`templates/outgoing-wifi-drop-dut.yaml`: classic ESP32, `outgoing_connection: delay: 2s`, a "Wifi cycle" button doing
`wifi.disable`, 4 s, `wifi.enable`, a `debug` loop-time sensor, millis from the wifi and api triggers. Flash by USB,
`serial_capture.py PORT SECONDS OUT --ms`, press the button N times 40 s apart with `api_button.py DEV_IP KEY press
"Wifi cycle"`, then `wificycles.py CAPTURE LABEL` prints per drop the Wi-Fi-to-dial gap, dials and dials closed, who
connected first, and the loop-time maxima. For the loop cost of polling, repeat with an unreachable
`host: 192.0.2.1`. Home Assistant refuses a dial-in while its own session is READY (`Refusing dial-in adoption`),
so after a drop every dial is closed and HA's incoming reconnect wins 11 to 33 s later; the measurable effect is the
first dial landing in the loop the network flag flips.
Results: https://github.com/esphome/esphome/pull/21085#issuecomment-6254742791

## User defined action listing and execution without a client library (#18882)

`scripts/api/rawactions.py HOST list` speaks the plaintext native API over a raw socket and decodes every
`ListEntitiesServicesResponse` by field number (argument fields 1 name, 2 type, 3 description, 4 example, 5 optional,
6 default_value), so new wire fields show up without waiting for the client library. `rawactions.py HOST exec ACTION
k=v ...` sends `ExecuteServiceRequest` with `sint32` ints (field 5, not the legacy int32 field 2), fixed32 floats,
length delimited strings and `int[]` as repeated sint32 (field 7); `ALLOW_MISSING=1` leaves arguments out on purpose
(the device then drops the call). `templates/api-actions-dut.yaml` (D1 mini, three actions, optional variables with
defaults) and `templates/api-actions-strings-dut.yaml` (required strings) log what arrived. String arguments are
`StringRef`s that are not null terminated: copy them with `snprintf("%.*s", size, c_str())` into a buffer before
logging, and do not pass `%.*s` to `logger.log`, whose validator counts it as one pattern.
