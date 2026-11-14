# bluetooth_proxy

## Held GATT link after a half-open API session (#21256, #22077)

When Home Assistant stops answering without closing its socket, the proxy must not keep the BLE link it opened for
that session. `scripts/bluetooth_proxy_half_open/` reproduces it with:
- `proxy.yaml`: the proxy under test on a classic ESP32 devkit (ESP-IDF, VERBOSE logger with the busy tags at DEBUG,
  encrypted API). Build it from the tree under test with a `secrets.yaml` (`wifi_ssid`, `wifi_password`, `api_key`).
- `peripheral.yaml`: a second ESP32 (an S3 here) as a peripheral that only advertises while nobody is connected
  (`esp32_ble_server` takes one client by default), with manufacturer data `FF FF 21 56` so the client can find it.
- `ha.py`: the Home Assistant stand-in (aioesphomeapi, `client_info` "Home Assistant test"). Roles: `owner` connects
  to the peripheral through the proxy and idles, `observe` counts the peripheral's adverts and the slot report and can
  ask for a connect at the end, `swap` connects at once and retries on the first advert. Every role stays subscribed
  to advertisements: with no subscriber the proxy's loop disconnects every link at once, which hides the bug (an
  early run failed with `disconnect while backend idle` for that reason).

`cases.sh PROXY_HOST LABEL` (env `PROXY_PORT`, `PERI_PORT`, `SC`, `PY`, `API_KEY`) captures both boards and runs
case C (old client killed, new client after 4 s), case H (old client frozen with `SIGSTOP`, so the socket stays open
with no FIN; new client after 4 s, watched 300 s, then it asks to connect and releases) and case N (frozen and nobody
reconnects for 200 s). `cases.sh PROXY_HOST LABEL swap` instead repeats H three times with the new client
connecting at once (`ha.py ... swap ADDR`) and retrying on the first advert. Both end with PASS or FAIL: the cases
fail when an observer exits non-zero, never sees the peripheral advertise (the held link) or the proxy resets, and
swap fails when a round never connects. An owner that never got the link also fails (a rig problem, not a result).

- The proxy drops a frozen session as unresponsive after about 144 s.
- Before #22077, case H holds the link for good: 0 adverts, and the address stays in the slot report. With #22077 the
  link is released in the same millisecond as the `replaces` warning. In case 4 the first request logs `Connection
  request ignored, state: DISCONNECTING` (connected=false at 0.2 s) and the retry connects at 0.5 to 0.7 s.

## Out-of-order GATT discovery responses (#22518)

Some peripherals answer a characteristic discovery (Read By Type) with records out of handle order. ESP-IDF 5.5.6
rejects that (`gatt_process_read_by_type_rsp invalid handle ...`) and ends the whole discovery; 5.5.5, Android and
BlueZ accept it. `scripts/att_reorder/` reproduces it with no such device:

- `server.yaml` (ESP32-S3) serves `FFF0` with six characteristics, then `1826`.
- `proxy.yaml` (classic ESP32, `idf` substitution; keep per-build files such as `example-proxy.yaml` in the same
  directory) links `components/attswap`, which wraps Bluedroid's
  `l2c_rcv_acl_data` (`-Wl,--wrap`); `attswap.hpp` swaps the first two records of the response whose first record
  declares `FFF1`, and logs a swap count.
- `gattget.py PROXY PEER_MAC ROUNDS` reads the services through the proxy like Home Assistant (no cache);
  `run.sh PROXY_YAML [ROUNDS]` flashes both boards and prints the services per round and the discovery errors.

On 5.5.6 Home Assistant gets `fff0` with no characteristics and no `1826`, while the proxy still logs
`Service discovery complete` (the proxy ignores the error status); on 5.5.5 every service arrives. To find the order a
real device uses, decode an Android HCI snoop: a Read By Type response lists its records in the order sent.

- Handles, reads and notifies after a reordered response: flash `server-notify.yaml` (`SERVER_YAML` on `run.sh`) and run
  `gattcheck.py PROXY PEER_MAC`. It prints every service, characteristic and descriptor handle, reads FFF1, FFF2 and
  FFF3 by the received handles (each holds its own value), and enables notify on FFF1, FFF2 and 2ACD. Compare the table
  with a run of a proxy built without `attswap`. On ESP-IDF 5.5.5, which accepts the unsorted response, FFF1's
  descriptor range runs over FFF2's declaration, value and CCCD, so FFF2 has no CCCD (#22576).
- `bluetooth_gatt_start_notify` only registers the notify on the proxy; the client writes the CCCD itself, as Home
  Assistant does. Without that write no notification arrives.
- Give the proxy an `api` encryption key (`API_KEY` for the scripts). A Home Assistant on the same network connected to
  a plaintext test proxy and took over its Bluetooth subscription (`Subscription from Home Assistant ... replaces
  aioesphomeapi`), dropping the test client's connection mid-run.
