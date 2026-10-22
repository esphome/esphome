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
