# esp32_ble_server pairing and encrypted characteristics (#17562)

`scripts/ble_server_security`: a classic ESP32 devkit runs the secure GATT server (`server.yaml`) and a second ESP32
(an S3 here) runs `ble_client` as the central (`client.yaml`). The server has a `read_encrypt` battery level
(`99.9`), a plain characteristic, a `write_encrypt` characteristic that logs what it receives, a `read_encrypt_mitm`
characteristic and a `read_encrypt` characteristic that notifies every 2 s. `run.sh TAG IO AUTH` builds both with the
pairing mode as substitutions, pairs once, reconnects twice, and `summary.py` prints PASS or FAIL. The server needs
`esp32_ble_server` security from #17562 or later: point `SERVER_TREE` at that checkout (the client builds from
`TREE`, dev is fine). `summary.py` needs each refused read to show an `Error reading char` reply, so a run where the
client never connected fails instead of passing.

- `ble_client` never starts pairing on its own (a phone does, after an `insufficient authentication` reply); the
  client's `Pair` button calls `BLEClientBase::pair()`. Read errors show as `Error reading char at handle N, status=5`
  (not authenticated: no bond) or `status=15` (not encrypted: bond known, link not encrypted yet).
- The `ble_client` text sensor subscribes once, at connect, before any pairing, so a protected characteristic refuses
  it. The `Subscribe` button writes the CCCD again after pairing; `ble_client` drops its service list after discovery,
  so it writes the characteristic handle + 1 (where ESP-IDF's GATT server puts the CCCD).
- Numeric comparison (`display_yes_no` with `sc_mitm_bond`) tells a fresh pairing from a bond reuse: the
  `numeric comparison` line only appears on a fresh pairing. Just Works cannot show it.
- ESP-IDF 5.5.6 drops the bond on the second disconnect on both roles (`Deleting unpaired device`), so that reconnect
  pairs from scratch; 5.5.5 keeps it (`-s idf 5.5.5`).
- The server keeps its bonds in NVS. One holding an authenticated bond for the client refuses a Just Works re-pairing
  silently; erase the devkit's flash before switching from nc to jw.
- `summary.py` failed both modes at 58636914ab: `read_encrypt_mitm` was readable with no pairing, because
  `BLEService::create_characteristic()` took an 8 bit `esp_gatt_char_prop_t` and dropped the MITM bits (1 << 8 and
  1 << 9). Check the generated `create_characteristic` call and the property type when a flag above bit 7 is added.

## ESP32 central against the secure server (#17562, #22081)

`scripts/ble_server_security/esp32central-*` swaps the roles: an ESP32-S3 runs the secure server
(`esp32central-srv.yaml`: plain, encrypted, MITM read and write, and an encrypted notify characteristic, numeric
comparison accepted automatically) and a classic ESP32 devkit runs `esp32central-cli.yaml` (`ble_client` with
`display_yes_no` and `sc_mitm_bond`, text sensors that log every value, a MITM write and a link cycle every 40 s).
`bonds.hpp` logs the bond list Bluedroid loaded from NVS on both boards.

- The server never starts pairing (a peripheral only answers a security request), and `ble_client` does not start
  it on an insufficient authentication reply (status 5) the way Android does. The client's `on_connect` calls
  `esp_ble_set_encryption(remote_bda, ESP_BLE_SEC_ENCRYPT_MITM)` instead.
- `esp32central-flow.sh TAG` erases both boards, flashes them, captures a fresh pairing with three link cycles
  (phase A), then resets both together so each loads its bond (phase B), and prints per phase the connects, numeric
  comparisons (a fresh pairing), MITM reads, server writes and `Deleting unpaired device` lines. It passes when
  phase B reads the MITM characteristic with no numeric comparison and no `Deleting unpaired device` line.
- On ESP-IDF 5.5.6 every other reconnect pairs again (`Deleting unpaired device` on the disconnect after a bonded
  reconnect), the regression #22081 describes.

## Bluedroid hex parsing bench (#22081)

`scripts/bluedroid_hex_bench/`: `hexbench.hpp` times Bluedroid's `sscanf` parsing (`string_to_bdaddr` and the
`btc_config_get_bin` loop) against the parsers in #22081's `esp32_ble/bluedroid_hex.h` on the same inputs, valid and
invalid, and checks that the outputs match. `run.sh LABEL` (env `TREE`, `PORT`, `SC`, `PY`, `BOARD`) builds
`bench.yaml`, flashes, captures 25 s and fails on any mismatch or a missing `DONE`. Classic ESP32: about 7 to 9 times
faster for addresses, 6 to 7 times for 16 and 28 byte keys.

## ble_client on_notify with a real server (#21821)

`scripts/ble_notify/`: `server.yaml` (ESP32-S3 GATT server; a numeric and a text characteristic notify every 2 s,
with uint16 frame and empty notification rounds, set through `id(c)->set_value(...)` and `notify()` in a lambda) and
`client.yaml` (classic ESP32 client with two `on_notify` triggers per sensor, one of them delayed, and a link cycle
every 60 s). `run.sh LABEL` (env `TREE`, `SERVER_PORT`, `CLIENT_PORT`, `SERVER_BLE_MAC`, `SC`, `PY`) flashes both,
captures the client 150 s and counts `on_notify` against `on_value` per sensor.

- `run.sh` also requires `All clients established, services released`: before #21821 it never appears with a text
  sensor `on_notify`. The delayed txt `on_notify` may miss only the last notify of the capture.
- The server takes one connection. Park any other client that would connect to it with
  `esptool --after no-reset chip_id` (the board stays in the bootloader).
- An ESP32-C6 devkit whose native USB port refuses esptool can be flashed and logged through its UART bridge port
  with `logger: hardware_uart: UART0`.
