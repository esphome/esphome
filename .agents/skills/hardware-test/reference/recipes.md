# Proven test flows

Each flow names the pull requests it was used on, so their result tables show what to post.

## Captive portal `/wifisave` hammer (#20802, #20784)

Reproduces a `/wifisave` arriving during the STA cooldown, and proves a fix on every board.
Device under test: `templates/example-dut.yaml` (bogus network, `ap:` with `ap_timeout: 15s`,
`captive_portal:`). Client: `templates/example-client.yaml` on an ESP32 or a D1 mini; it joins the
device's AP, posts 15 bogus saves 3 s apart, then the real credentials. With the portal up the cooldown
is 30 s, so nearly every save lands inside one. Driver: `scripts/wifisave_hammer.sh`. Settle 40 s on
ESP boards, 90 s on the BW15 (its first attempt on the real network fails, see boards.md).

Counts that matter: `Requested WiFi Settings Change` (saves received), `Exiting cooldown early`, crash
markers, boots (`Log initialized`), and the client's `bogus save status 200` and `real save status 200`.
A healthy run: every save answered 200, one cooldown exit per save (two on the BW15, whose retry enters
the adapter restart), no panic, and the real credentials connect the device within about 10 s.

Fault injection for AP start failures: wrap `this->ap_setup_ = this->wifi_start_ap_(this->ap_)` in
`setup_ap_config_()` with an `HWTEST_AP_FAIL_COUNT` build flag
(`esphome: platformio_options: build_flags: -DHWTEST_AP_FAIL_COUNT=2`), raise the portal's "Captive
portal started" from LOGV to LOGI, and after provisioning curl the device's STA address: connection
refused proves the portal's server is gone.

## `reboot_timeout` with a fallback AP (#9958)

Only one board runs the AP at a time, or stations join the wrong one. AP probe: bogus network,
`ap: {ssid, password, ap_timeout: 5s}`, `reboot_timeout: 60s`, `logger: level: VERBOSE` (ESP32 and
ESP8266 log `AP client connected MAC=` at verbose), `on_boot: logger.log: BOOT`. Station probes:
`reboot_timeout: 0s`, `interval: 150s` running `wifi.disable`, `delay: 20s`, `wifi.enable` (60 s cycles
are too short to cover the due moment). Pass pattern: "Can't connect; rebooting" exactly 60 s after boot
with no station joined, no reboot for minutes while one is, then a reboot within a second of the last
disconnect. The BW15 restarts its AP on every STA retry and cannot be tested this way.

## MQTT (#20433, #17935, #20762, #20767, #20768)

A local mosquitto with `allow_anonymous true`, `log_type all` and `log_dest file`: the broker log gives
each subscription's QoS, the device address (`New client connected from <ip> as <name>-<mac>`) and exact
subscribe counts. Kill and restart mosquitto to exercise `on_disconnect` (reason 0) and resubscription.
Keep `reboot_timeout: 0s` on `api:` and `mqtt:`. The ESP-IDF outbox swings free heap about 2 kB right
after bulk subscribe and unsubscribe; read the heap seconds later over several rounds before calling a
leak. Spawned subscriptions need about 2.5 s to reach the broker before the next publish.
`esphome logs --device MQTT` only uses the MQTT transport when the config has no `api:`. Driver pattern: a
paho script publishes and polls the serial capture file.

## UDP, multicast, sockets (#20556 to #20559, #20585, #20590)

A listener on port 9 needs no privileges on macOS. Test multicast device to device; host to device
multicast never arrives on some access points, and `239.255.x.x` groups are unreliable even to a stock
listener. Broadcast to boards is lossy under power save, unicast is not. Flood at a fixed packet rate
(the host hits `ENOBUFS` unthrottled); 1500 packets per second survived on every board. A forced Wi-Fi
reconnect on the Pico keeps IGMP membership. Set a long loop interval from `wifi.on_connect`
(`App.set_loop_interval(20000)`), not `on_boot`, or Wi-Fi connect crawls. The `logger.log` action
rejects `%.*s`; use a lambda.

## OTA overlap and lock (#20588), OTA timeouts (#19041), deflate (#19037)

All overlap orders (a web upload during a native OTA and the reverse) must be refused; the lock release
after an interrupted web upload waits about 14 s (ESP8266) or 7 s (ESP-IDF) for disconnect detection. A
refused web client on `web_server_idf` never gets the busy reply; the connection drops at reboot.
`pkill -f "a\|b"` does not alternate; use two pkills.

## DHCP lease cache (#17767)

Cycle 20 s awake, `deep_sleep.enter 30s`, wake, 20 s, `App.safe_reboot()`, logging `millis()` at the
`wifi.connected` condition. An RTS hardware reset must fall back to DHCP; a software reboot and a deep
sleep wake must apply the cache. `esphome run --no-logs` silently skipped the upload on one board; use
`upload` then `logs`.

## HDMI-CEC two-board wire (#9189)

Two boards on one wire: promiscuous mode, a 0x8F request answered with 0x90 0x00, a `destination:`
filter on the reply handler, template buttons for each target plus lambdas calling
`send(source, dest, span)` as another initiator. Healthy numbers: device-side turnaround 15 to 22 ms,
round trip 144 to 156 ms. Press buttons over the API; judge timings from device logs (ESP32 light sleep
lags the press).

## LD2450 polygon zones (#20154)

To make a target "leave" without anyone moving, set the radar's Zone-1 numbers over the target and
switch the zone type to `Filter` (the count drops to 0 within 0.5 s); set it back to `Disabled` after.
Writing those numbers blocks the ld2450 loop about 50 ms.

## Scheduler and loop wake A/B (#20356)

An external component whose template button arms an SDK `os_timer` (50 ms, one shot) on the ESP8266;
the callback runs in SYS context, records `millis()` and calls `set_timeout` or `defer`; the timeout
logs the elapsed time and loop count delta. Drive with an aioesphomeapi script (`subscribe_logs` plus
`button_command`). Build the same yaml twice with different names, one per tree.

## Noise handshake and OTA encryption on the ESP8266 (#19062, #19233)

Host side timing is useless on a weak link; bench on the device, minimum of five runs over two boots.
Provision a key with `noise_encryption_set_key` (aioesphomeapi); `espota2.run_ota(..., noise_psk=key)`.

## ESP-NOW on the ESP8266 (#14407)

The NONOS SDK only delivers broadcast ESP-NOW frames while the station is associated or a soft-AP is
up; a hidden, open, client-less soft-AP makes them arrive. In AP-only mode it sends from the soft-AP MAC.

## Wi-Fi Easy Connect (DPP) enrollee (#17934)

Only an Android phone configures: Settings, network details, Add device, Share Wi-Fi (the Wi-Fi list's
QR scanner makes the phone an enrollee). Render the URI with the `qrcode` module. `esp32: framework:
log_level: DEBUG` plus `sdkconfig_options: CONFIG_WPA_DEBUG_PRINT: y` prints the supplicant state machine.

## nRF52840 dongle boards (#19009, #20814)

Test config: the two LEDs as inverted GPIO outputs, the button as an inverted pull-up binary sensor, a
10 s alive interval. To retest the nrfutil download path, move the cached `sdk-nrf/nrfutil` aside first.
