# wifi

## `reboot_timeout` with a fallback AP (#9958)

Only one board runs the AP at a time, or stations join the wrong one. AP probe: bogus network,
`ap: {ssid, password, ap_timeout: 5s}`, `reboot_timeout: 60s`, `logger: level: VERBOSE` (ESP32 and
ESP8266 log `AP client connected MAC=` at verbose), `on_boot: logger.log: BOOT`. Station probes:
`reboot_timeout: 0s`, `interval: 150s` running `wifi.disable`, `delay: 20s`, `wifi.enable` (60 s cycles
are too short to cover the due moment). Pass pattern: "Can't connect; rebooting" exactly 60 s after boot
with no station joined, no reboot for minutes while one is, then a reboot within a second of the last
disconnect. The BW15 restarts its AP on every STA retry and cannot be tested this way.

## DHCP lease cache (#17767)

Cycle 20 s awake, `deep_sleep.enter 30s`, wake, 20 s, `App.safe_reboot()`, logging `millis()` at the
`wifi.connected` condition. An RTS hardware reset must fall back to DHCP; a software reboot and a deep
sleep wake must apply the cache. `esphome run --no-logs` silently skipped the upload on one board; use
`upload` then `logs`.

## Wi-Fi Easy Connect (DPP) enrollee (#17934)

Only an Android phone configures: Settings, network details, Add device, Share Wi-Fi (the Wi-Fi list's
QR scanner makes the phone an enrollee). Render the URI with the `qrcode` module and open the PNG.
`esp32: framework: log_level: DEBUG` plus `sdkconfig_options: CONFIG_WPA_DEBUG_PRINT: y` prints the
supplicant state machine.

## WISP bridge with an ethernet upstream (#7147)

`templates/wisp-ethernet-dut.yaml` (ESP32-S3 with a W5500, `wifi: ap:` with `ap_mode: WISP` and no station) and
`templates/wisp-client.yaml` (D1 mini joining the bridge, `http_request` to a web server on the wired LAN and to
`http://example.com/` every 15 s, `time: sntp`). Secrets: `wisp_psk` and `lan_http` (serve a text file on the host
with `python -m http.server 8787 --bind 0.0.0.0` from a scratch directory). Judge by the client's `GET status 200`
lines, the web server log showing the device's ethernet address as the source (NAT), the client's DHCP info (DNS1 is
the LAN resolver) and `SNTP synced`. The device logs `Starting AP` and `DHCP DNS server: <resolver>` on its USB
console; flashing over the air reboots it for repeat boots. Before the AP address and DHCP server moved ahead of the
AP start (the fix in that PR), the first AP start failed on every boot with `esp_netif_dhcps_option failed: 20484`
(DHCP server already started by the AP start event) and `Failed to enable NAT`, and the bridge only came up on the
retry 90 s later; a client that cannot find the SSID for the first minute on an older build is that. With the fix
the bridge is up 6 s after boot. Any change to the ESP-IDF AP start should also rerun the captive portal hammer.

## Reboot against a WPA2/WPA3 PMF access point (#21590)

- AP: `scripts/wpa3_ap/wdap.yaml` on a classic devkit. ESPHome `wifi: ap:` only, then an on_boot lambda switches it
  to `WIFI_AUTH_WPA2_WPA3_PSK`, PMF capable, and registers a WIFI_EVENT handler that logs `STA JOIN` and
  `STA LEAVE ... reason N` (the include header pulls in `esp_wifi.h`). It disables Wi-Fi after 420 s.
- AP leave reason 3 means the station sent a deauth; reason 2 means the AP found a stale association when the
  station came back; several reason 2 before one join are rejected reconnects.
- DUTs (`wdbk.yaml` BK7238, `wdcb.yaml` CB3S, both on `wd-common.yaml`) list the AP at priority 10 and the main
  network at 0, and reboot 30 s after joining the AP and 150 s after joining the main network. So they move to the
  AP while it is up and fall back to the main network (where they can be flashed over the air) once its window
  ends. `run.sh BUILD` flashes the AP, does OTA for the CB3S and a serial flash for the BK7238, then captures one AP
  window; `an.py` prints each station's join and leave sequence and fails on a rejected reconnect, an AP that did
  not switch to WPA2/WPA3 (`AP CONFIG FAILED`), a DUT missing from the AP events (`DUT_MACS`) or a BK7238 that never
  joined the AP.
- Gotchas: the LibreTiny build directory has two `.uf2` files, so flash `firmware.uf2`. The BK7238 serial flash
  sometimes fails to link while the chip is in a reboot loop; retry it 3 or 4 times. OTA fails if the DUT reboots
  mid-upload; retry.
- Finding: BDK 3.0.78 rejects about 2 reconnects per reboot when the station left without a deauth; BDK 3.0.33
  does not.

## WISP and ALWAYS_ON AP rig (#20077)

`scripts/wisp_ap`: the upstream is an ESP32-S3 with Ethernet running a WISP bridge (`wisp-upstream.yaml`) with its AP on
10.40.0.1/24 and web buttons `ap off` and `ap on`. Its subnet must not be 192.168.4.0/24, the DUT AP's default, or
nothing routes. The DUT is a classic devkit (`dut-wisp.yaml` or `dut-always-on.yaml`); the client is a D1 mini on
the DUT's AP (`wispcli.yaml`) logging its DNS server, a name lookup, a GET by name, the AP itself and a LAN file by
IP. `run.sh DUT START SECONDS T:STATE...` switches the upstream AP at the given times and `sum.py` groups the client
results by phase. Capture the D1 mini without `--reset` (a CH340 floods garbage after an RTS reset). Clients keep
the captive portal's DNS answer for its TTL.

## AP restart with WPA3 mixed and LIGHT power save (#21531)

`scripts/wifi_restart`: the WISP upstream (`wisp-upstream-wpa3.yaml`) switched to WPA2/WPA3 mixed by a script that
runs again after every `ap on`, DUTs (`rebk.yaml`, `recb.yaml` on `re-common.yaml`) with a static IP on the bridge's
subnet, a fallback AP, `power_save_mode: LIGHT` and syslog to the host through the NAT. `run.sh BUILD` switches the
AP off and on three times; `an.py` fails on a reboot, a crash or a DUT that is not back on syslog within 120 s of an
AP start. After 19 minutes the DUTs run `wifi.configure` to the main network; that keeps the static IP, so the next
OTA needs the host to have an address on the bridge's subnet as well (an interface alias). The CB3S log is on UART1
(TX1), so its configs need `logger: hardware_uart: UART1`. Runners must `pkill` only their own ports; a global pkill
stops the other runs' captures.

## Channel and network switching with a bench AP (#22142)

`scripts/wifi_channel/`: `bench-ap.yaml` makes an ESP32-S3 an AP-only bench network on a fixed channel (11 by
default; pick one your normal network does not use), and `dut-common.yaml` gives the device under test that AP at
priority 10 plus your normal network, logs every `wifi_info` ssid and channel update (with the channel the component
reports and whether it is connected), and cycles `wifi.disable` and `wifi.enable` every 90 s. `run.sh LABEL` (env
`TREE`, `PORT`, `AP_PORT`, `SC`, `PY`, and `BENCH_CHANNEL` when the AP is not on 11) flashes the device and captures 420 s while it takes the AP down with
`esptool --port <AP port> --after no-reset chip_id` (the board stays in the bootloader) and back with
`--after hard-reset`. The channel sensor must follow every move between the two networks: the run passes when the
connected channel starts and ends on the bench channel with another channel in between.

- Dropping the AP under a connected device gives a `Beacon Timeout` link loss.
- The 90 s interval fires once right after boot.
- A `Disconnected ... Association Leave` right after `Connecting to` is the aborted attempt, not a missed connect.

## Soft AP with the BLE tracker scanning (#22534, #22566)

`scripts/ble_scan_soft_ap/`: `dut.yaml` runs only `wifi: ap:` with `captive_portal:` and `bluetooth_proxy:`, and
`client.yaml` (on a second ESP32) joins that AP and fetches the portal page every 5 s, logging `JOIN` and `STAT`
lines. `run.sh` erases and flashes the DUT, resets the client and prints the generated scan window, the first join, the
page counts and the client's disconnect reasons (usage in its header).

- A scan window equal to the scan interval (320 ms of 320 ms) leaves the radio no time for the soft AP: on dev no
  client joined on the classic ESP32, S3, C3 or C6. With #22566 the tracker uses its 30 ms connection window while
  the AP runs, and every chip took a client in about 4 s.
- Only one AP with the test SSID may be up at a time; list the other AP boards in `PARK` so they wait in their ROM
  bootloader.
- To check that the full window comes back after provisioning, log `esp32_ble_tracker` at VERBOSE and provision
  with `templates/client-wifisave.yaml` (set `ap_ssid` to the DUT's and `ap_password: ""`, the DUT's AP is open): `Connection or AP active, using 48 unit scan window` must stop once the AP stops.
- Some clients do not join an ESP8266 AP at all (the C6 failed on dev too with `Too Many Associations`); use a C3.

## ESP8266 power save after captive portal provisioning (#22542, #22564)

The DUT runs `ap:` plus `captive_portal:` with no station network and logs `wifi_get_sleep_type()` every 5 s (0 is
`NONE_SLEEP_T`, 2 is `MODEM_SLEEP_T`); an ESP32-C3 with `templates/client-wifisave.yaml` posts the real credentials.
Erase the flash before each run so no saved credentials skip the portal. On dev the sleep type stayed 2 after
provisioning although the config asked for none; with #22564 it read 0 from the moment the AP started, and the
`networks:` path still read 0.
