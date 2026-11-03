---
name: hardware-test
description: Test an ESPHome change on real boards over USB. Use this whenever a task asks to test, verify, reproduce, bench, A/B or soak a pull request, a branch or a crash on hardware, or mentions flashing, serial logs, esphome upload, a captive portal or fallback AP test, or results to post on a PR. Covers finding the serial ports, building from the right checkout, flashing each chip family (including the LibreTiny kits that stock tools cannot flash), bounded log capture, proven test flows and how to report results.
allowed-tools: Read, Write, Edit, Bash, Glob, Grep
---

# Hardware testing

The goal of a hardware test is evidence a reviewer can trust: which build ran on which board, what was
driven, what the logs show, and the same flow on a `dev` build when a quirk could be pre-existing.
`$SKILL` below is this directory (`.agents/skills/hardware-test`).

## Ground rules

1. **Serial ports cannot be shared.** One bounded capture per port, stopped before any flash on that
   port, never an open-ended `esphome logs`. Stop a capture with `pkill -f "[s]erial_capture.py <port> "`,
   naming the port so a capture on another board survives; the bracket keeps the pattern from matching
   the shell that runs it. When more than one agent session shares the bench, run a copy of the script
   under a session-specific name (`cap<topic>.py`) so another session's `pkill` cannot take your capture
   down. Never run `esphome logs` in a `while true` loop: the loop respawns it and holds the port.
2. **Credentials stay local.** Put a `secrets.yaml` next to the scratch configs and reference it with
   `!secret`. A copy of a device's own config keeps using the secrets file next to that config: two
   checkouts can hold different networks, and the wrong one leaves the device unreachable after the OTA. Never paste a config with credentials into a PR, never commit scratch configs or logs.
3. **Build from the tree under test.** `python -m esphome` puts the current directory first on
   `sys.path`, so run it with the checkout or worktree as the working directory and pass the yaml by
   its absolute path. A venv `esphome` entry point may import an installed snapshot instead of the
   checkout. Confirm with `python -c 'import esphome; print(esphome.__file__)'`, and for an A/B grep
   the build's `src/` for the change before trusting any number. Tool calls sent together share one
   shell, so a `cd` in one can land another's compile in the wrong tree: pass `PYTHONPATH=<worktree>` on
   every compile and upload command and print `esphome.__file__` into the compile log.
4. **PR build and baseline build get different `name:` values.** Builds share
   `.esphome/build/<name>`, so the same name means `esphome upload` can flash the wrong binary. Saved
   Wi-Fi credentials are keyed by the config hash when the config has `networks:`, so a test that must
   start unprovisioned gets a fresh name every run. A config with `ap:` only and no `networks:` uses one
   fixed key shared by every such config on the board, so a new name does not help there: wipe once at
   boot with an `on_boot` lambda that resets the preferences and saves a marker key
   (`templates/dut-ap-only-wipe-once-bk72xx.yaml`).
5. **Flash and RAM deltas are measured against the PR's merge base** (`git merge-base origin/dev HEAD`
   checked out in a throwaway worktree), with the baseline commit named in the table. When a delta
   looks odd, diff symbol sizes (`nm -S -C --size-sort` from the matching toolchain) before quoting it.
6. **Prove a quirk is pre-existing before blaming the PR:** run the identical flow on a `dev` build in
   the same session and say so in the results.
7. **Pipe safely.** Set `PYTHONUNBUFFERED=1` on any `esphome run` or `esphome logs` whose output is
   piped, or lines land minutes late. Bound every long command with `timeout` (coreutils) or
   `perl -e 'alarm shift; exec @ARGV' <seconds> <command>` on macOS without it.

## Workflow

1. **Find the boards:** `python $SKILL/scripts/ports.py` (any Python with pyserial, for example the
   ESPHome venv). Port names follow the USB hub port and change on every replug, so never reuse a name
   from an earlier session. CH340 adapters (D1 mini, BW15 kit, BK7238, CB3S) all look the same; tell
   them apart with a 10 s capture using `serial_capture.py --reset` (ESP8266 prints its ROM banner at
   74880 baud, LibreTiny boards print `[LT]` and ESPHome lines, BK7238 prints its bootloader version).
   `ports.py --wait 2e8a` blocks until a Pico re-enumerates.
2. **Make a scratch directory** outside the checkout, with `secrets.yaml` and the templates you need.
   One yaml per board per variant, file name equal to `name:`.
3. **Compile** in the right tree:
   `(cd <worktree> && PYTHONUNBUFFERED=1 timeout 1500 python -m esphome compile <abs yaml> > <name>-compile.log 2>&1)`.
   Keep the RAM and flash lines at the end. Two ESP-IDF or two LibreTiny builds at the same time race
   over tool caches; run them one at a time.
4. **Flash** per chip family (table below). Stop the capture on that port first.
5. **Capture** with `serial_capture.py PORT SECONDS OUT` in the background, then poll the file with
   `grep` in an `until ... sleep 2` loop with a cap. USB CDC boards (Pico, nRF52, native USB ESP32)
   drop the port on reboot; the script reopens it until its deadline.
6. **Drive** the test: a second board running a client config, `curl`, an aioesphomeapi script that
   presses a template button and subscribes to logs, MQTT publishes, or `interval:` and `on_boot:`
   automations in the config itself.
7. **Analyze** with `grep -a -c` counts over the capture and keep an `-events.txt` extract of the lines
   that matter. Decode crashes with the platform's `addr2line` (see `reference/boards.md`).
8. **Report** on the PR. Every result comment starts with this two-line GitHub alert, then a blank line:

   ```
   > [!NOTE]
   > Automated testing results
   ```

   Then one short sentence saying what was tested (builds and SHAs, board, what was driven), a results table
   (build SHA, board and framework, what was driven, counts, result, plus the `dev` control row), short bullets
   for the findings, and the setup and exact flow in a collapsed `<details>` block. Answer any question asked on
   the PR directly in the comment. No credentials, SSIDs, IP addresses, port names or other bench details.

## Flashing by chip family

| Board | Flash | Log |
|---|---|---|
| ESP8266 D1 mini (CH340) | `python -m esphome upload <yaml> --device <port>` | `serial_capture.py` at 115200; `esphome logs` on some CH340s emits garbage after the RTS reset. A failed esptool write can wedge the adapter until the board is replugged. `esptool erase_flash` before a test that needs an unprovisioned device: the preference sector survives a reflash |
| ESP32 devkit (CP2102 or CH340) | `esphome upload --device <port>`, ESP-IDF or Arduino | `serial_capture.py`. Default Wi-Fi light sleep delays API driven actions by seconds; judge timing from device logs. `power_save_mode: none` for OTA overlap tests |
| ESP32-S3, C3, C6, H2 native USB | `esphome upload --device /dev/cu.usbmodem*` | `logger: hardware_uart: USB_SERIAL_JTAG`. The USB console can wedge when the app takes over the console (0 bytes, esptool `No serial data received`); replug, or watch through `esphome logs --device <name>.local` |
| Pico W / Pico 2 W | `esphome upload --device <port>` (1200 baud touch); first flash ever via BOOTSEL and `--device /Volumes/RPI-RP2` or `RP2350` | port re-enumerates after upload: `ports.py --wait 2e8a` then capture |
| BW15 kit (RTL8720CF, rtl87xx) | `~/.platformio/penv/.libretiny/bin/python $SKILL/scripts/flash_rtl87xx_kit.py flash write -d <port> <build>/.pioenvs/<name>/*.uf2`. Plain `esphome upload` and plain ltchiptool time out because opening the port asserts DTR/RTS and holds the chip in reset | `serial_capture.py --released`, never `esphome logs` |
| BW12 and other AmebaZ modules (RTL8710BX, rtl87xx) on a plain USB-UART | `~/.platformio/penv/.libretiny/bin/python -m ltchiptool flash write -B 115200 -b 115200 -t 120 -d <port> <uf2>`, started first, then strap by hand: CEN to GND, TX2 to GND, release CEN, release TX2. Without `-B 115200` the system data read at 1.5 Mbaud fails on CH340s (`Data read error at 0x9000`) and nothing is written. A 1 MB image takes about 80 s. A development board with DTR and RTS wired to CEN and the download pin needs no strap | `serial_capture.py`; a reset pulse does not reach CEN on a plain adapter, so a capture starts mid-run |
| BK7238 / CB3S (bk72xx, RTS on CEN) | `~/.platformio/penv/.libretiny/bin/python $SKILL/scripts/flash_bk72xx.py flash write -d <port> <uf2>` (about 75 s) | logs are on UART2 by default; `logger: hardware_uart: UART1` puts them on the flashing adapter. `serial_capture.py --reset` gives a chip-enable reset; a plain open may or may not reset it, check for the bootloader banner |
| LN882H | ltchiptool forces 1 Mbaud for writes; weak adapters need `LN882H_YM_BAUDRATE` patched to 115200 in the penv's `ln882htool.py`; short BOOT to GND while the tool connects | OTA after the first flash |
| nRF52840 Dongle (PCA10059) | Nordic `nrfutil nrf5sdk-tools dfu usb-serial` on the bootloader port, or `dfu: true` in the config so the 1200 baud touch pin-resets into the bootloader; adafruit-nrfutil is refused | app has two CDC ports, ESPHome log on the second |
| Adafruit-style nRF52 (UF2 bootloader) | `UF2BOOT` volume or adafruit-nrfutil; `dfu: true` works through GPREGRET | single CDC port |
| Any reachable device | `esphome upload <yaml> --device <ip or name.local>`; `--file <bin>` for a prebuilt image | `esphome logs --device <ip>` bounded with `timeout` |

ESP-IDF OTA image: `.esphome/build/<name>/build/firmware.ota.bin`. LibreTiny OTA port is 8892.

## Scripts in `$SKILL/scripts`

- `ports.py`: list ports with VID:PID, product, serial and a board guess; `--wait VID[:PID]`,
  `--product NAME`, `--serial SN` (one specific board, whatever path it comes back under) and
  `--serial-of PORT` (the serial number behind a path).
- `serial_capture.py PORT SECONDS OUT [--baud] [--released] [--reset] [--append] [--ms]`: bounded timestamped
  capture (`--ms` adds milliseconds for timing work), ANSI stripped, `LOG_SECRET` text written as `<secret>`, reopens a vanished port, exits at the
  deadline. OUT is truncated unless `--append` is given, so counts over it cover one run.
- `flash_rtl87xx_kit.py`: ltchiptool wrapper for the BW15 kit whose CH340 drives CEN (RTS) and the boot
  strap (DTR). `flash read` needs the family `realtek-ambz2`; `flash write` does not.
- `flash_bk72xx.py`: ltchiptool wrapper for BK72xx boards whose RTS drives CEN. It misses the link now
  and then; retry up to three times.
- `api_button.py HOST KEY list|press NAME [...]`: list a device's template buttons or press them over the API; the
  recipes drive their test configs with it.
- `wifisave_hammer.sh KIND DUT_PORT DUT_YAML DUT_TREE CLIENT_PORT CLIENT_YAML CLIENT_TREE [SETTLE]`:
  the fallback AP plus captive portal hammer end to end (compile, flash, capture, counts) with the
  templates below. KIND is `esp`, `pico`, `bw15` or `bk`. It ends with `RESULT PASS` (exit 0),
  `RESULT FAIL` (exit 3: a crash, a reboot after the AP came up, or the device never joined the network),
  `RESULT INVALID`
  (exit 2: a precondition was missed, the stimulus was insufficient, or a capture did not last the run)
  or exits 1 when a build, flash or port step failed.

Per-topic scripts, each described in the matching `reference/recipes/<component>.md`: `online_image/`
(server with prefixed misbehaviour cases, PNG, JPEG, BMP and QOI generators, API drivers, first pass timer), `zwave_proxy/` (checksum verifier, device info), `usb_cdc_acm/`
(host driver, Zephyr driver probe), `rtsp_server/` (protocol client, raw rate reader, measurement set,
API image grab), `cse7766/` (API observer with ping), `zephyr_coredump/` (reader flow, decode with the
Zephyr tools), `libretiny_prefs/` (rtc preference harness header), `libretiny_crash/` (BK72xx crash probe header,
capture decoder, RTL8720C ROM dump versus report matcher), `api_outgoing/` (wake cycle and Wi-Fi drop timing readers), `ld2450/` (zone number driver, loopback harness driver with number and binary sensor states and TX frame summarizer, frame stream harness with presence hold and link loss drivers, BT MAC driver), `api/` (raw plaintext native API client for user defined actions, store_yaml recovery client over Noise or plaintext), `http_request/` (update manifest server, update state driver, request test server, action case driver, concurrency, reboot, update check and OTA drivers, `ota_auth/` OTA request cases with basic auth), `modem/` (button and sensor driver over the plaintext API, also used for the BLE MIDI boards), `dfrobot_sen0395/` (API driver and case runner), `homeassistant_light/` (direct bulb driver, held-report case driver), `usb_audio/` (URL player), `speaker_media_player/` (API player driver, pipeline task case runner), `web_server/` (SSE client, request mix, SSE and HTTPD case runners), `mqtt/` (broker config with TLS, client-certificate and WebSocket listeners, test CA with server and client certificates, transport case runner, flash-and-drive runner, CLI TLS check without a board, MAC suffix runner with per-variant configs for two boards, `shutdown/` shutdown message runner and matcher), `udp/` (datagram recorder, `udp.write` payload checker and case driver), `entity_icons/` (icon and device class readback over the API, MQTT discovery, web_server and dump_config, with a comparer), `uart_sim/` (API driver and flash, capture and drive runner for the simulated UART sensors), `remote_base/` (loopback runner and API log decoder per sent frame), `xiaomi_ble/` (frame and header generator with the expected counts, injecting, advertising and receiving configs, counter and runner), `ble_terminate/` (proxy and peer configs, connect and disconnect driver, peer disconnect matcher, runner), `i2c_slave/` (D1 mini slave and BK7238 master configs), `wpa3_ap/` (PMF AP with join and leave logging, BK7238 and CB3S DUTs, runner and summary), `wisp_ap/` (WISP upstream with AP buttons, WISP and ALWAYS_ON DUTs, client, runner and per phase summary), `wifi_restart/` (WPA3 mixed WISP upstream, static IP DUTs with syslog, runner and summary), `ota_timing/` (OTA upload timer with deflate, raw and TCP modes, API build check, A/B runner and median summary, `inflate_bench/` on-device inflate benchmark with stream generator, configs and syslog runner), `gpio_hold/` (ISR pin hold self test with ESP32 and S3 configs), `ble_event_inject/` (GAP and GATTC event injector header and proxy package), `light_sleep_wake/` (LEDC wave and edge timer for light sleep wakeup on a C3), `manual_ip/` (station and AP manual IP configs, DNS probe and runners), `web_handlers/` (late handler, web_server, captive_portal and prometheus variants, web OTA runner), `bk72xx_flash/` (flash readback with retries, per partition diff of two dumps), `ble_server_security/` (secure GATT server and pairing client configs, driver, summary and runner), `prometheus/` (datetime metrics configs and driver), `zephyr_ble_server/` (nRF52 server, looping ESP32 client and UF2 flash and count runner).

## Templates in `$SKILL/templates`

- `dut-fallback-ap.yaml` (package): a device that never finds its network, falls back to its AP and
  serves the captive portal. `example-dut.yaml` is the per-run file with the platform block.
- `client-wifisave.yaml` (package): joins that AP, posts N bogus `/wifisave` requests, then the real
  credentials from `!secret`. `example-client.yaml` is the per-run file. `bogus_saves: "1"` gives a
  plain provisioning check.
- Per-topic device configs, each described in `reference/recipes/<component>.md`: `online-image-dut.yaml`,
  `usb-cdc-acm-dongle-dut.yaml`, `nrf52-preferences-dut.yaml`, `rtsp-waveshare-s3-eth-dut.yaml`,
  `waveshare-s3-eth-camdiag.yaml`, `s31-cse7766-dut.yaml`, `nrf52-coredump-crash-dut.yaml`,
  `bk72xx-rtc-prefs-dut.yaml`, `bk72xx-crash-probe-dut.yaml`, `dut-ap-only-wipe-once-bk72xx.yaml`,
  `homeassistant-light-dut.yaml`, `deep-sleep-outgoing-dut.yaml`, `outgoing-wifi-drop-dut.yaml`,
  `globals-storage-dut.yaml`, `usb-audio-waveshare-s3-eth-dut.yaml`, `usb-audio-sendspin-s3-eth-dut.yaml`,
  `cast-pro-1-sendspin-control.yaml`, `aux-2-preprod-stock-dut.yaml`, `aux-2-preprod-usb-audio-dut.yaml`,
  `dfrobot-sen0395-dut.yaml`, `aux-2-preprod-speaker-player-dut.yaml`, `web-server-sse-dut.yaml`,
  `mqtt-transport-dut.yaml`, `mqtt-tls-dut.yaml`, `udp-write-dut.yaml`, `entity-icon-dut.yaml`,
  `online-image-s3-eth-dut.yaml`,
  and the directories `store_yaml/` (package with secrets one directory up, per-board files),
  `http_request_update/` (manifest URL setter shapes, per-board files), `modem/` (modem only and modem plus
  Wi-Fi boot checks), `ble_midi/` (fake BLE MIDI instrument and the client), `http_request_action/` (request
  action package with per-board files and an OTA flash variant) `uart_sim/` (simulator header, common package, per-build parts and run files) and `remote_base/` (common
  package, transmit script, per-board templates and the ESP8266 software sender).

## When the PR keeps moving

Record every head in the stack (`gh pr view N --json headRefOid`), build, and recheck before flashing; if anything
moved, wait five minutes and check again so the test runs on a settled head. When a head moved only by merging dev,
compare the PR's own diff (`git diff $(git merge-base OLD origin/dev) OLD` against the same for NEW); if it is
identical and the dev commits do not touch the code under test, earlier results still describe it, but rebuild so the
comment quotes the exact commits. Fetch `origin dev` before computing a merge base.

## When something looks wrong

- A capture stopped early or a line is missing: check the timestamps before trusting it.
- A script that pipes into `tee` never ends although its work is done: a background command written as
  `(cd dir && server & echo $!)` backgrounds the whole `cd && server` list, so a shell stays behind holding the
  pipe open and `$!` is that shell, not the server. Write `(cd dir && exec server > out 2>&1 < /dev/null) &`.
- The client never joins the AP: another board may still run an AP with the same SSID, or the device
  booted with saved credentials because the name was reused. One AP device at a time on the bench.
- Builds come out byte-identical between branches: the wrong tree was imported (rule 3). The reverse
  too: two binaries that differ only in the build timestamp and in `__LINE__` values of log calls below
  the changed code are the same build; map the offsets with `objdump -h` and `addr2line` first.
- A boot the host cannot capture (a power cut, an OTA-only board): keep the boot report in a flash
  preference and repeat it in a periodic alive line, as `scripts/libretiny_prefs/ltprefs.hpp` does.
- Multicast from the host never arrives on some access points: test it device to device. Broadcast
  is lossy under Wi-Fi power save, unicast is not. Throttle UDP floods to a fixed packet rate.
- The merge base comes from a freshly fetched `origin/dev`; a stale local `dev` puts dependency bumps into the
  PR diff and builds the control from the wrong commit.
- A `.local` name can stop resolving in the middle of a run; keep the address from the boot log and drive by it.
- Never point `framework: source:` at a local `file://` copy of LibreTiny to try a patch: PlatformIO installs it over
  the shared `~/.platformio/platforms/libretiny`, so every later build on the machine uses the patched copy. Patch
  through the ESPHome side (a linker `-Wl,-wrap,<symbol>` or a define) instead, and if it happened, restore the
  directory from a clean copy of the same release.
- `wc -l` pads its count on macOS, so `tail -n +$n0` needs `$((n0+0))` or an arithmetic expansion.
- `api:` reboots a board after 15 min without a client: `reboot_timeout: 0s` on `api:` and `mqtt:` for
  soak tests.

`reference/boards.md` has per-chip facts and crash decoding; `reference/recipes/` has the proven
flows. Add new findings there.
