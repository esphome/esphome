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
