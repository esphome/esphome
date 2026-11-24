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

## GATT UUID forms through a proxy (#22679)

`scripts/gatt_uuid/`: `server.yaml` (the `ble_notify` server plus a service with 128 bit, 32-bit base and 16 bit
UUIDs on characteristics and descriptors, and 2901 and 2902; set the name with `-s name`), `gattlist.py PROXY PEER
OUT.json` (lists every UUID, reads, notifies 10 s, retries 0x3E), `cmp.py REF.json OTHER.json` (entry by entry diff,
exits 1 on a difference) and `run.sh NAME` (Pico W proxy from `rp2_web_ble/picow.yaml`, list, compare). Take the
reference from an ESP32 proxy (unset `API_KEY` for a plaintext API).

- On rp2 before #22679, BTstack truncated 32-bit base UUIDs with a nonzero low half to 16 bits (`5ec10001-...` came
  out as `00000001-...`); a zero low half (5ec10000) came through intact.
- Connects through the Pico W proxy fail with 0x3E now and then (see `reference/boards.md`); the scripts retry.

## Scan complete never arrives (#22770)

`scripts/ble_scan_complete/`: `scan.yaml` is a proxy config like the report's (ESP32-C6, interval = window = 1100 ms,
debug sensors, `api: reboot_timeout: 0s`, a `duration` substitution, 5 s by default; the validator wants at least
three intervals, so 2 s is refused). The tracker watchdog is `duration * 2`, so a short duration shortens it too.
`scaninst.yaml` adds `scan_inst.hpp` and five `-Wl,--wrap` flags that count one scan's duration timer through
Bluedroid: `BTM_BleScan` starts and stops, `btu_start_timer(BTU_TTYPE_BLE_SCAN)`, the timer's alarm firing (a shim on
its `osi_alarm_new` callback, BTC task), `btm_ble_timeout` (BTU task) and the INQ_CMPL handed to `btc_transfer_context`
(PID 5, act 3; the param is 108 bytes in libbt, so do not compare with the C++ `sizeof`). A 30 s interval logs them as
`scaninst`; a lost completion shows where the chain stopped (timer > fire + 1, fire > timeout, timeout > cmpl_post, or
cmpl_post above the ESPHome-side `ends / 2`). `ha.py HOST OUT SECONDS [--toggle S --jitter J]` is the Home Assistant
stand-in (raw adverts, scanner state, set mode passive once, optional active/passive toggles). `soak.sh TAG HOST
SECONDS [ha.py options]` (env PORT, API_KEY or else `apikey.txt`) runs a bounded serial capture plus ha.py.
`wait.sh CAPTURE SECONDS [FROM_LINE]` blocks until a timeout, drop or unexpected-state line and prints the counters.
- Bluedroid and IDF log lines reach the ESPHome log as tag `esp-idf` at VERBOSE (`[V][esp-idf:000][BTU_TASK]: E ...`),
  so a user capturing at INFO over the API never sees a Bluedroid error.
- A stop from Home Assistant landing just after the timer fired (toggle period 5.03 s with 0.08 s jitter against 5 s
  scans hits it about once per 20 minutes) gives `Unexpected state: STOPPING on scan complete`, `stop complete -
  status 1`, `BTM_BleScan scan not active`; the tracker recovers. A stop while STARTING logs `Cannot stop scan:
  STARTING` and the mode change waits for the next restart.
- With about 55 devices around (about 95 adverts/s), no lost completion in about 2800 natural 5 s cycles on 2026.10.2
  (VERBOSE and INFO builds) plus about 670 toggled stops, and none on dev.

## Connection interval: third link refusals and discovery time (#22859)

`scripts/ble_conn_interval/`: `proxy.yaml` is the reporter's ESP32-C3 proxy (scan interval = window, active, 5 slots;
substitutions `flags` for `CONFIG_BT_BLE_50_FEATURES_SUPPORTED` and `window`) with `ivlog.hpp`, which wraps
`esp_ble_gattc_register_callback` and `esp_ble_gap_register_callback` (`-Wl,--wrap`) and logs `IVLOG connect ...
interval=N` for every new link and `IVLOG update ...` for every conn param update, so the interval each build really
used is in the serial log. `proxy-e32.yaml` and `proxy-s3.yaml` are packages of it for a classic ESP32 and an S3, adding
`coexlog.hpp`. `proxy-weak.yaml` lowers the BLE TX power on every power type (`txlvl`, `esp_power_level_t`)
to approximate a weaker link without moving boards. `gen_peri.py OUT NAME VARIANT BOARD SERVICES CHARS` writes an
`esp32_ble_server` peripheral with a 2901 description per characteristic and one writable "first command"
characteristic (raises `CONFIG_BT_GATT_MAX_SR_PROFILES` to 32 and the attributes to 500); build three of them, a
mid size one, a large table (26 x 4) and a small one, and name them in `PEERS=A,B,C`.
`drive.py HOST scan|third|disc|eve ...` is the aioesphomeapi driver (see its docstring); `suite.sh TAG SCENARIO...`
runs the scenarios in chunks (env HOST, PEERS, PY, API_KEY or `apikey.txt`) and `report.py TAG CAPTURE` summarises
them, counting `Rejected: Resources` lines from the serial capture. `devtime.py CAPTURE [FROM] [TO]` gives device-side
link and discovery times per attempt, `summary.py TAG:SCENARIO[:SLOT]...` and `pool.py LABEL TAG:SCENARIO...` the
min/median/p95/max over one or several runs (they read `cap-TAG.txt`), and `macdisc.py NAME ROUNDS` times a real
device's cold discovery with bleak for reference.

- `COLD=1` makes `drive.py` call `bluetooth_device_clear_cache` before each attempt. Without it the proxy's Bluedroid
  NVS cache (`CONFIG_BT_GATTC_CACHE_NVS_FLASH`, set by bluetooth_proxy) answers discovery from flash, and a
  29 service table "resolves" in 0.1 to 0.3 s even with `has_cache=False` on the API side.
- In V3 without cache the proxy sends `connected` only after discovery, so the API's connect time is link plus
  discovery; a peer that drops the link during discovery shows as a first callback with connected false, err 19
  (`Remote closed during discovery` in the device log).
- Open a held link in a timing scenario with `has_cache=True`; holding a large table peer otherwise means a full
  discovery first, which at slow intervals runs past the 20 s connect timeout.
- A C3 with scan window = interval starves Wi-Fi: `TCP buffer full` deferrals, dropped clear cache replies and the
  API connection dropping mid-run are normal there; `suite.sh` reruns in chunks until it has the rounds.
- The proxy answers nothing (clear cache times out, connects are cancelled at once) unless the client keeps a raw
  advertisement subscription open for the whole session; every driver here does.
- Raw adverts: a device advertising both a shortened (0x08) and a complete (0x09) name must be matched on the
  complete one (the Eve Flare puts "Eve" first).
- An `esp32_ble_server` peer's BLE address is its Wi-Fi MAC plus 2.
- A conn param update while two other links are held comes back as status 19 and the interval stays; alone the same
  update succeeds. It happened with every peer, so it is the C3 controller, not the peripheral.
- Results on the C3 (dev e5728407db): a no-cache third link (FAST, 7.5 ms fixed) with two held is refused by the
  controller (`BT_HCI: opcode=0x200d, status= 0d: Rejected: Resources`, `Connection open failed, status=133`) in 11/12
  and 8/12 rounds; cached (MEDIUM) thirds are not. Same without BLE 5.0 features and with a 10 % scan duty. With
  0x18/0x28 nothing is refused, but cold discovery of a 12 service / 35 characteristic table as the third link takes
  21 to 23 s on the device instead of 4 to 5 s, and a 29 service / 112 characteristic table about 62 s instead of 13 s.
  A dev third link that is not refused can still take 0.6 to 18 s to come up.
- Range variants: opening uncached links on 0x06..0x09 gives 11.25 ms every time (the C3 picks the maximum) and no
  refusals; asking for 0x06/0x06 at CONNECT_EVT is accepted when the link is alone and refused (status 19, interval
  unchanged) when two others are held, so under load it behaves like the range.
- A HomeKit lamp (Eve Flare) asks for its own interval (18 to 24, 22.5 to 30 ms) about 0.4 s after connect on both
  builds and drops the link (0x13) about 10 s after connect with no HAP traffic; through the C3 its cold discovery (10
  services, 57 characteristics) takes 8.3 to 9.7 s, so it rarely finishes.
- `proxy-weak.yaml` with `txlvl: 4` (-12 dBm) left a C6 peer unable to connect at all; `txlvl: 7` (-3 dBm) worked and
  roughly doubled discovery time. Never call `esp_ble_tx_power_set` from `on_boot`: it faults in `ble_txpwr_set` (boot
  loop); the yaml applies it from an interval once the controller is enabled.
- API-level times through the C3 swing 2x between runs of the same build (Wi-Fi), so compare builds on the device-side
  numbers and treat API medians as context only. A long run needs a fresh TAG per flash: `cap-TAG.txt` is truncated on
  reuse.
- Classic ESP32 boards with this scan config lose 80 to 90 % of pings and drop off Wi-Fi; measure them with
  `proxy-e32-selftest.yaml` instead.

### Coexistence

- On the C3 with ESP-IDF 5.5.6, `esp_coex_preference_set()` is a stub (`coex_preference_set` in libcoexist is
  `li a0,0; ret`), so esp32_ble_tracker's PREFER_BT/BALANCE switching does nothing there. The working lever is
  `esp_coex_status_bit_set(ESP_COEX_ST_TYPE_BLE, ESP_COEX_BLE_ST_MESH_CONFIG)` (only 0x08, 0x10 and 0x20 are accepted
  for BLE); read it back with `extern "C" uint32_t coex_schm_status_get(uint32_t type)` (type 1 = BLE).
  `coex_status_get` and `coex_schm_status_bitmap_get` do not show it.
- #22904 holds that bit from an uncached connect to search complete, counted across slots; compare builds before
  and after it. Holding it cut device-side discovery 10 to 19 % and changed nothing for refusals or the Eve Flare.
- `coexlog.hpp` wraps `esp_coex_status_bit_set` and `_clear` and logs the BLE status read back: 0x8 held and 0x6
  cleared on the C3 and S3, 0xa and 0x2 on a classic ESP32. `proxy-e32-selftest.yaml` checks the readback with no API
  client at all, through the #22904 acquire and release calls.
- A proxy with no USB console (an S3 on PoE): `apilogcap.py HOST SECONDS OUT` writes API log lines stamped with the
  local receive time so `devtime.py` works, and `apirun.sh NAME YAML TAG SCENARIO...` OTA flashes it, logs and pings it
  (`pinger.py`) while `suite.sh` runs. `sidefx.py TAG` compares ping RTT inside the uncached discovery windows with
  outside them and counts `TCP buffer full` lines and API closes.

## ESP-IDF version A/B of a proxy, and notification bursts (#22962, #22955)

`scripts/bt_proxy_ab/` compares two proxy builds with the same scenarios. `proxy-common.yaml` is the proxy package
(active connections, the `ivlog.hpp` connection interval logger from `ble_conn_interval/`, `heaplog.hpp`), with
`proxy-c3.yaml`, `proxy-s3e.yaml` (Waveshare ESP32-S3-ETH, W5500 first, Wi-Fi second) and `proxy-s3w.yaml` (Wi-Fi only)
on top. `heaplog.hpp` logs internal and total heap every 30 s and a `HEAPMIN` line on every 1 KB drop of the internal
low-water mark. The servers: L is a large table with a burst service, built with
`../ble_conn_interval/gen_peri.py OUT bpab-l ESP32S3 esp32-s3-devkitc-1 26 4 --burst`; M is a mid size `gen_peri.py`
server; the third is a real HomeKit device (`HK_NAME`). Set `PEER_L`, `PEER_M`, `HK_NAME` and `PEER_ADDRS=L=MAC,M=MAC,
eve=MAC` (see `peers.py`); passive scans see no scan response, so `drive.py` also matches the servers by address.

- `drive.py HOST adv|disc|third|notify|churn|eve ...` is the aioesphomeapi driver (see its docstring); it keeps the
  ff01 to ff03 handles of each server in `handles.json` so cached connects skip discovery. `suite.sh TAG HOST
  PORT|api [steps]` runs the steps with a serial log capture (or `apilogcap.py` for `api`) and a pinger, `chain.sh
  NAME:YAML:TREE:FLASHDEV:LOGPORT[:NSEC[:STEPS]]...` flashes, resolves `NAME.local` and runs the suite per build, and
  `report.py TAG` gives per step the API results, device-side link and discovery times from the capture, heap, warning
  and error counts and ping. `cross.sh PORT YAML NAME TREE_A TREE_B` alternates two builds on one board (idle ping,
  then the advert test). `stress.sh HOST SECONDS` adds Wi-Fi load with eight large pings.
- `selftest.hpp` with `selftest-e32.yaml` runs the same scenarios on the device with three `ble_client`s (V3
  connection types through the tracker's DISCOVERED path) for a board whose Wi-Fi cannot carry the API test; set
  the three addresses with `-s mac_l ... -s mac_m ... -s mac_eve ...` and the ff01 handles of L and M from
  `handles.json` with `-s handle_l ... -s handle_m ...`. `stparse.py CAPTURE` summarises it. Its phase table is a
  fixed copy of the `suite.sh` steps; change both when a scenario changes.
- Bursts: `burst.py HOST RUNS OUT [COUNT PER_TICK]` writes [count_hi, count_lo, per_tick] to fe03 on L, counts the 2
  byte sequence notifications on fe01 and the final notification on fe02, and records gaps; the final value carries the
  server's own send failures (`server_send_failed_min`, refused sends plus late failures counted by `srvconf.hpp`; a
  lower bound), so they are not taken for proxy loss. `ndrop.hpp` with `ndrop-pkg.yaml` prints temporary drop counters
  added to `BluetoothConnection::on_notify_data` in a local tree.
- ESP-IDF 5.5.6 against 6.1.1 showed no Bluetooth difference on a classic ESP32, C3, S3 (Wi-Fi and W5500) or C6. Heap
  low-water drops land at "Service discovery complete" of a large table (the Bluedroid cache build, about 15 KB)
  and under a TCP backlog, on both versions.
- A bench Wi-Fi can degrade over hours (a C6 control build without Bluetooth went from 2 % to 30 % ping loss);
  compare builds only with back-to-back crossed runs.
- Bursts are lost at the API send gate (refused frames), not in the BLE stack. Ethernet on an Olimex ESP32-POE
  delivered 200/s and 400/s; W5500 at 400/s blocks the loop and logs "Dropped N BLE events".
  `network: enable_high_performance` on a C3 proxy under a burst ate the heap (9 KB free, low-water 392 B) and once
  aborted in Bluedroid (`host_recv_pkt_cb hci_hal_h4.c:656`, osi_calloc failed).
- An S3 GATT server logging on USB-Serial-JTAG boot-looped right after "Log initialized" (ISR assert in
  `usb_serial_jtag_isr_handler_default`, then LoadProhibited) once its USB console had been unread for hours. A
  recovery image with `hardware_uart: UART0` that calls `esp32::crash_handler_clear()` broke the loop, so the burst
  server logs on UART0. Not reproduced on purpose; a replug would tell whether the host side was stuck.
- A board that joins only at -73 to -76 dBm may just be badly oriented; check the RSSI before blaming a build.
