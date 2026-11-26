---
name: hardware-test
description: Test an ESPHome change on real boards over USB. Use this before a pull request that changes what a device does (a driver, a bus, a platform, a fix for a fault seen on a device) is opened or marked ready for review, and whenever a task asks to test, verify, reproduce, bench, A/B or soak a pull request, a branch or a crash on hardware, or mentions flashing, serial logs, esphome upload, a captive portal or fallback AP test, or results to post on a PR. Covers finding the serial ports, building from the right checkout, flashing each chip family (including the LibreTiny kits that stock tools cannot flash), bounded log capture, proven test flows and how to report results.
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
   A capture started with `&` inside a compound command can outlive its deadline and hold the port; check
   `pgrep -fl "[c]ap<topic>"` before flashing.
2. **Credentials stay local.** Put a `secrets.yaml` next to the scratch configs and reference it with
   `!secret`. A copy of a device's own config keeps using the secrets file next to that config: two
   checkouts can hold different networks, and the wrong one leaves the device unreachable after the OTA.
   Generate an `api_key` with `openssl rand -base64 32` when you have none. Never paste a config with
   credentials into a PR, never commit scratch configs or logs.
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
   When a change moves storage between the heap and static RAM, the static number alone misleads: also log
   the free heap at the end of setup (`on_boot` at priority -100) over three boots of each build and report
   the total in use (static RAM plus heap used).
6. **Prove a quirk is pre-existing before blaming the PR:** run the identical flow on a `dev` build in
   the same session and say so in the results.
7. **Pipe safely.** Set `PYTHONUNBUFFERED=1` on any `esphome run` or `esphome logs` whose output is
   piped, or lines land minutes late. Bound every long command with `timeout` (coreutils) or
   `perl -e 'alarm shift; exec @ARGV' <seconds> <command>` on macOS without it. Save an exit code with `rc=$?`
   right after the command: in `echo "$(date +%T) rc=$?"` the command substitution runs first and `$?` reads 0.
8. **Give builds a private PlatformIO directory when sessions may use different Pythons.** ESPHome stamps the
   PlatformIO core directory with the Python version that last built, and a build from another version wipes and
   reinstalls the shared packages under every other build. Set `PLATFORMIO_CORE_DIR=<scratch>/pio` on every
   compile and upload (`scripts/pio_build.sh` does). Old releases and framework A/B builds need one too
   (see `reference/boards.md`).
9. **One ESP-IDF or LibreTiny build at a time on a shared machine.** Concurrent builds race over tool caches. Wrap
   compiles in a lock (`BUILD_LOCK` for `scripts/pio_build.sh`, for example a script that holds a `mkdir` lock).
10. **Check that the stimulus ran.** A load or soak run where most requests failed was not loaded, and a capture
    that stopped early covers nothing; check the driver's own counters before reading the device's verdict.

## Workflow

1. **Find the boards:** `python $SKILL/scripts/ports.py` (any Python with pyserial, for example the
   ESPHome venv). Port names follow the USB hub port and change on every replug, so never reuse a name
   from an earlier session. CH340 adapters (D1 mini, BW15 kit, BK7238, CB3S) all look the same; tell
   them apart with a 10 s capture using `serial_capture.py --reset` (ESP8266 prints its ROM banner at
   74880 baud, LibreTiny boards print `[LT]` and ESPHome lines, BK7238 prints its bootloader version).
   `ports.py --wait 2e8a` blocks until a Pico re-enumerates.
   A change in validation or platform independent code needs no board: a `host:` build serves the API on
   port 6053 on every interface (reach it at 127.0.0.1, and add `api: encryption:` on a shared network); see
   recipes/homeassistant.md for one against a throwaway Home Assistant.
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
8. **Report** as a comment on the PR, never in its description; when testing before the PR exists, keep the report and
   post it right after the PR is opened. Every result comment starts with this two-line GitHub alert, then a blank line:

   ```
   > [!NOTE]
   > Automated testing results
   ```

   Then one short sentence saying what was tested (builds and SHAs, board, what was driven), and on the next line a
   bold result, one of **Test result: PASS**, **Test result: FAIL**, **Test result: PASS WITH CAVEATS** or
   **Test result: UNCLEAR**, followed by one sentence with the key numbers (for example "the device closed none of 32
   streams, dev closed 12 of 16"). FAIL means a regression or a fix that does not work. PASS WITH CAVEATS means it works
   but something about the change itself needs a look; name the caveat in that sentence. A limit of the test (the
   original crash did not reproduce) or a device or network quirk the tooling reported correctly is not a caveat on
   the change; say it as a separate note. UNCLEAR means the run could not decide; say what is
   missing. A reader must not have to dig through the table or guess. Side findings that do not change the result (a
   bug already on `dev`, a one-off oddity) go in the bullets, marked as separate from the result.

   Then a results table
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
- `apiconn.py`: `connect(host, tries, port, on_stop)` (API connect and login with retries, `API_KEY` from the env),
  `log_text()` (an API log line without ANSI codes), `stop_event()` (an Event set when the connection ends),
  `ble_connect()` (wait for an advert, connect through a proxy without a cache like Home Assistant) and
  `enable_notify()` (start notify and write the CCCD), imported by the API drivers.
- `pio_build.sh compile|upload NAME YAML [DEVICE] [esphome options]`: build or upload YAML as `-s name NAME` from
  `TREE` with a private PlatformIO directory, an import check and an optional `BUILD_LOCK`; logs to `NAME-*.log`.
- `resolve.sh NAME [TRIES]`: the address of `NAME.local` once it resolves and answers a ping.
- `wifisave_hammer.sh KIND DUT_PORT DUT_YAML DUT_TREE CLIENT_PORT CLIENT_YAML CLIENT_TREE [SETTLE]`:
  the fallback AP plus captive portal hammer end to end (compile, flash, capture, counts) with the
  templates below. KIND is `esp`, `pico`, `bw15` or `bk`. It ends with `RESULT PASS` (exit 0),
  `RESULT FAIL` (exit 3: a crash, a reboot after the AP came up, or the device never joined the network),
  `RESULT INVALID`
  (exit 2: a precondition was missed, the stimulus was insufficient, or a capture did not last the run)
  or exits 1 when a build, flash or port step failed.

Per-topic scripts, each described in the matching `reference/recipes/<component>.md`:

- `ac_dimmer/` (ESP8266 zero cross and gate timing loop harness with noise injection, per window and per phase
  summaries, runner with a web_server load)
- `api/` (raw plaintext native API client for user defined actions, store_yaml recovery client over Noise or plaintext,
  `overflow/` API overflow buffer counters with a sensor config generator)
- `api_client_synced/` (on_client_synced config generator, API client and reboot timing drivers, late fire, shutdown and
  argument summaries, Home Assistant token renewal)
- `api_outgoing/` (wake cycle and Wi-Fi drop timing readers)
- `app_lists/` (17 type entity package with sub-devices and areas, API listing and command driver, `/events` and
  dump_config normalisers)
- `att_reorder/` (Read By Type reorder wrapper for the proxy, GATT servers, service reader, table, read and notify
  check, runner)
- `automation_trace/` (automation marker trace driver and runner)
- `bk72xx_flash/` (flash readback with retries, per partition diff of two dumps)
- `ble_adv_triggers/` (advertiser with six raw frames, ESP32 and BK72xx receivers with eleven triggers, runner and
  checker)
- `ble_conn_interval/` (proxy configs with the connection interval and coexistence loggers, GATT peripheral generator,
  scenario driver and suite, device-side timing and side effect summaries)
- `ble_event_inject/` (GAP and GATTC event injector header and proxy package)
- `ble_notify/` (ble_client on_notify server and client)
- `ble_scan_complete/` (Bluedroid scan timer instrumentation and Home Assistant stand-in)
- `ble_scan_soft_ap/` (AP with bluetooth_proxy, portal client, runner)
- `ble_server_security/` (secure GATT server and pairing client configs, driver, summary and runner)
- `ble_server_stack/` (esp32_ble_server retest configs with runtime service control and service growth inside
  `on_write`, Improv, direct and proxy GATT clients, flow runner)
- `ble_terminate/` (proxy and peer configs, connect and disconnect driver, peer disconnect matcher, runner)
- `bluedroid_hex_bench/` (Bluedroid hex parsing bench)
- `bluedroid_mtu_recursion/` (central and peer configs for the queued MTU and execute write recursion, analyzer)
- `bluetooth_proxy_half_open/` (held GATT link after a half-open API session)
- `bmi270_real/` (real BMI270 probe header, variants, runner and summaries)
- `bmp390/` (config generator, probe and fault injection header, runner and stats)
- `bt_proxy_ab/` (proxy A/B packages with a heap logger, scenario driver, suite, flash chain and crossed runner,
  on-device self test, burst client and drop counters, reports)
- `canopen/` (CANopen SDO client suite header, client, server with slow objects and MCP2515 node configs, action paths
  package, bus check, two-board capture with a reset hold)
- `cc1101_long/` (long packet and lambda length driver for two CC1101 radios, begin_tx in packet mode, async serial OOK
  package and driver)
- `cc1101_sleep/` (register and PATABLE snapshot header, sleep and setter checks)
- `climate_ir_fuzz/` (climate IR decoder fuzz for the C++ unit tests)
- `climate_ir_loop/` (climate IR loopback over the API: step driver, frame splitter and comparer, one-pin loopback board
  packages for the ESP32 and the D1 mini, whirlpool per-model runner)
- `crash_report/` (crash buttons, report fetch over the API)
- `crash_report/quiet_then_subscribe.py` (drains a crash record, then subscribes to logs only after N quiet reboots)
- `crc32/` (vector header and package)
- `cse7766/` (API observer with ping)
- `dfrobot_sen0395/` (API driver and case runner)
- `display_recorder/` (recording fake display component, test image writer, scene package, host checker)
- `ds248x_sim/` (DS2482-800 simulator, configs, driver)
- `entity_icons/` (icon and device class readback over the API, MQTT discovery, web_server and dump_config, with a
  comparer)
- `entity_transport/trcheck.py` (remote entity values against the sender's)
- `esp8266_edges/` (GPIO edge logger and 8N1 decoder, `jsn_sr04t` probe and counters, I2C bus sniffer and decoder, SDA
  unstick, VL53L1X register logger)
- `esp8266_flash_prefs/` (Wi-Fi scan versus flash erase repro, interrupt load during the flash sync, reset and restore
  driver, API hold, crash, heap and pulse rate summaries)
- `esp8266_lock_load/` (DS18B20 search and one-pin IR DUTs with a busy-wait control, UDP and HTTP load, runner)
- `espnow_link/` and `tcp_link/` (packet_interface links over ESP-NOW, UDP and TCP with checked streams)
- `ezo_fake_bus/` (fake EZO I2C bus and step config, runner)
- `flac_stream_changes/` (speaker media player with no DAC, stream change driver, error and heap summary, frame deltas,
  on-device FLAC decode benchmark with ESP32 and S3 configs, flow runner, hash check and PR against base comparer)
- `gatt_uuid/` (GATT UUID form server, lister and comparer through a proxy)
- `gpio_hold/` (ISR pin hold self test with ESP32 and S3 configs)
- `heap_alloc/` (operator new caller recorder, dump saver and decoder, cover cycle driver)
- `homeassistant_button/` (raw plaintext API client for action calls and wizard inputs, package and runner)
- `homeassistant_light/` (direct bulb driver, held-report case driver)
- `homeassistant_script_switch/` (throwaway Home Assistant config, host build, config and options flow, REST driver)
- `homeassistant_wizard/` (every-platform config with fixed and wizard-linked entities, raw API driver, light recheck,
  runner with heap readings)
- `http_request/` (update manifest server, update state driver, request test server, action case driver, concurrency,
  reboot, update check and OTA drivers, `ota_auth/` OTA request cases with basic auth)
- `https_idf/` (web_server_idf HTTPS test component, stalled TLS clients, certificate generator)
- `hub_input_sim/` (keypad, wiegand and rdm6300 simulation on a D1 mini, API log runner)
- `hx711/` (fake HX711 on the other core, value checker)
- `i2c_fault/` (NACK fault bus for vl53l0x, analyzer, runner)
- `i2c_isr_fault/` (software INA219 with seeded faults on the master's own pads, bus package, runner and summary)
- `i2c_recovery/` (wedge planting package with VL53L1X and Qwiic PIR files, ESP-IDF and RP2 recovery probes, scenario
  table)
- `i2c_slave/` (D1 mini slave and BK7238 master configs)
- `i2c_wire_ab/` (I2C wire token streams from host stubs, GPIO sniffers and an in-firmware target, `mux/` simulated
  TCA9548A host harness, device sweep, target and comparers)
- `i2s_loopback/` (I2S microphone loopback with a DC generator)
- `improv/` (Improv serial and BLE clients, serial scan reply dump, open AP config, package and runner, USB Serial/JTAG
  heap and log burst configs, console write stress, log burst checker)
- `interval/` (plain and delayed interval package, timing summary and runner)
- `ipv6_boot/` (milestone probe header with a netif fault injection wrap, base config with busy loop and injection
  add-ons, runner and analyzer)
- `ld2420_sim/` (fake LD2420 answering the hub's commands on a UART0 loopback, API state recorder, two-build comparer
  and runner)
- `ld2450/` (zone number driver, loopback harness driver with number and binary sensor states and TX frame summarizer,
  frame stream harness with presence hold and link loss drivers, BT MAC driver)
- `led_driver_sniff/` (SPI slave capture header with selftest, config generator, window comparer and runner)
- `length_frame_reader/` (ld2410 and tuya frame injection on a UART loopback, bare reader package, Run driver, real
  sensor soak)
- `libretiny_crash/` (BK72xx crash probe header, capture decoder, RTL8720C ROM dump versus report matcher)
- `libretiny_edges/` (pulse generator for BK72xx and BW15, RTL8710B pull-toggle edge and ISR read probe with two pin
  layouts, timing bench config, summary, AmebaZ OTA image RAM section check)
- `libretiny_prefs/` (rtc preference harness header)
- `light_sleep_wake/` (LEDC wave and edge timer for light sleep wakeup on a C3, external wave generator and per-call
  edge detail probe for peripheral power-down on the P4)
- `log_macros/` (log text A/B package and normaliser)
- `log_str_tables/` (fake I2C and UART devices, per-PR packages, voice_assistant driver, log normaliser, diffs and name
  coverage)
- `lognorm.py` (strips colours, timestamps and source line numbers from logs before a diff)
- `lt_platform.py` (newest LibreTiny platform for the flash wrappers)
- `lvgl_snapshot/` (LVGL snapshot and chart series dump header, package, PNG rebuilder)
- `lzss_link/` (packet_interface compression config generator, checked JSON, hex and binary streams with wire checks and
  codec timing, runner and summary)
- `manual_ip/` (station and AP manual IP configs, DNS probe and runners)
- `mcp23017/` (IOCON and INT test component, three interrupt wiring variants, runner)
- `mcp251xfd/` (CAN FD loopback stream, queue, TX object, forced mode, chip reset and CS counter harness, package with
  loopback, CLASSIC window and bus-off configs, runner)
- `mdns/` (unicast mDNS dump, hammer and latency)
- `modem/` (button and sensor driver over the plaintext API, also used for the BLE MIDI boards, AT commands for the UART
  passthrough; SIM7670G and SIM7080G CFUN fallback configs with a PPP traffic button, flash, reboot and disable flow,
  OTA flow, AT line filter, USB AT port client)
- `mqtt/` (`ha_discovery/`, `disable/`, `retained_burst/` and `reconnect/` runners; broker config with TLS,
  client-certificate and WebSocket listeners, test CA with server and client certificates, transport case runner,
  flash-and-drive runner, CLI TLS check without a board, MAC suffix runner with per-variant configs for two boards,
  `shutdown/` shutdown message runner and matcher, `disconnect_latency/` LibreTiny on_disconnect delay hook)
- `mqtt/dns_boot/` (broker lookup at boot by hostname, IP, unresolvable name and no DNS server, broker restart and Wi-Fi
  drop runners, analyzers)
- `npm1300/` (nPM1300 probe, every-entity and register dump configs, flash and capture runner)
- `nrf52_tcp_probe/` (TCP and UDP socket probe config, build and log check, API check over Thread)
- `nrf54_xiao/` (nRF54L preferences across a soft and a probe reset, debug entities, watchdog reset config)
- `nrf_pwm_probe/` (self-sampling zephyr_pwm duty and frequency probe, XIAO nRF54LM20A and ItsyBitsy sweeps, serial DFU
  runner)
- `online_image/` (server with prefixed misbehaviour cases, PNG, JPEG, BMP and QOI generators, API drivers, first pass
  timer)
- `openthread_race/` (border router restart race and Wi-Fi drop packages, ESP32-S31 Ethernet border router, PHY power
  link drop, restart loop with an OMR prefix sensor read over the API, meshcop check)
- `ota_automations/` (OTA automation package for both OTA platforms, four case runner, repeated web upload runner and
  per case summary)
- `ota_relay/` (serial to TCP relay header with frame checks and an AP drop op, host side, fallback AP and network drop
  DUTs, upload runners timed to retries and drops, per drop and per upload summaries)
- `ota_timing/` (OTA upload timer with deflate, raw and TCP modes, API build check, A/B runner and median summary,
  `inflate_bench/` on-device inflate benchmark with stream generator, configs and syslog runner)
- `output_button/` (pad edge timer header, button package and runner)
- `pico_i2c_target/` (Pico 2 W BME280/BMP280 I2C target and DUT configs, runner)
- `pio_probe/` (Pico 2 W PIO self-sampling probe, `color_correct/` colour correction cases, model and analyzer)
- `prometheus/` (datetime metrics configs and driver)
- `pwm_capture/` (RMT and PCNT period capture header, C6 config, API client with light transitions, runner and window
  totals)
- `remote_base/` (loopback runner and API log decoder per sent frame)
- `remote_base/pronto/`, `remote_base/hostharness/` and `remote_base/dumpall/` with `remote_base/ext/` and
  `remote_base/press.py` (Pronto dump A/B, host decode harness, every dumper over a saved corpus)
- `rmt_budget/` (RMT channel and memory budget case generator, runner, verdict)
- `rp2_sched_lock/` (IRQ context scheduler stress component, removed item race package, REST churn, loop timing and flow
  runner)
- `rp2_uart_loop/` (RP2 UART pin owner and in-chip loopback check, config generator, runner)
- `rp2_usbncm/` (RP2 USB NCM Ethernet: boot recorder build with panic capture, patched library and re-enumeration hammer
  configs, OTA and restart cycle runner, macOS host log capture and enumeration summary, event ring reader, proposed
  arduino-pico alt 0 patch)
- `rp2_web_ble/` (RP2040/RP2350 web_server, API and BLE proxy A/B: role configs, soak, SSE stall and churn runners,
  churn and never completed connect heap flows, pcap summary, GATT hold, fallback AP flow)
- `rs485_frame/` (simulated DLE/STX/ETX bus device, frame and gate cases, runner)
- `rtsp_server/` (protocol client, raw rate reader, measurement set, API image grab)
- `rtttl_edge_tap/` (note onset and release tap)
- `scd30/` (SCD30 package, clock stretch timer, SDA unstick, runner and counts)
- `sensor_filters/` (every filter shape with a fixed input sequence, per-board packages and runner)
- `serial_proxy/` (simulated serial device, raw API client, case suite)
- `sigma_delta/` (sigma_delta pin density probe, sequence generator, analyzer, runner)
- `signed_ota/` (C3 config with `verification_keys`, signing and CRC corruption script, OTA sequence runner)
- `sim_buses/` (PCF8574 and HD44780, MAX7219, SPI spy, packet_interface and SN74HC165 simulators, status LED probe, fake
  Home Assistant and wizard drivers)
- `smp_udp_ota/` (SMP reset fault injector, OTA runner, per mode result check, MCUboot swap limit, running image check
  and slot blanking UF2, Thread device config)
- `speaker_media_player/` (API player driver, pipeline task case runner)
- `speaker_tts_pause/` (streamed TTS with a pause)
- `spi_self/` (config generator for four boards, capture header, runner, checker and build diff)
- `static_buffer/` (`cg.static_buffer` probe components and config)
- `sun/` (longitude sweep package, runner and checker)
- `sx127x_sim/` (fake SX1276 and SX126x, two radios on a simulated air, SPI byte stream recorder and comparer)
- `tca9548a_real/` (mux scan, Wire transaction trace, soft watchdog runner)
- `tcpbench/` (TCP and UDP throughput component, host bench, P4-WIFI6 builds, round runner, summary, C6 co-processor
  update)
- `trf7962a_sim/` (fake TRF7962A behind the SPI bus with a scenario phase table)
- `tuya_sim/` (Tuya MCU simulator on a cross-wired UART, driver, runner and comparer)
- `twai_self/` (TWAI self-reception header, on_frame config and counter runner)
- `uart_button/` (UART0 loopback package, API driver with TX and RX counts, runner)
- `uart_parity/` (raw 8N1/8E1 termios reader, 8E1 receiver model, frame classifier, shelly_dimmer packages)
- `uart_sim/` (API driver and flash, capture and drive runner for the simulated UART sensors)
- `uart_sim/duco/` (Duco box model, config generator, runner)
- `uart_sim/pms_feed/` (PMS5003T feeder and capture on the logger port, power management variants)
- `uart_sim/virtual_uart/` (sds011 and mhz19 models on virtual_uart buses with overflow and ring checks, per platform
  configs, API capture)
- `uart_tx_break/` (TX break cycle components for ESP32, ESP8266 and LibreTiny with their configs)
- `udp/` (datagram recorder, `udp.write` payload checker and case driver)
- `usb_audio/` (URL player)
- `usb_cdc_acm/` (host driver, Zephyr driver probe)
- `value_range/` (on_value_range sequence with a restart, runner)
- `vl53l1x/` (speed sensor timeline package, range-to-speed checker)
- `vl6180x/` (probe, config generator with register, XSHUT and injection extras, runner and analyzers)
- `voice_announce/` (slow announce on the Voice PE)
- `voice_wake_word_audio/` (wake word audio capture and stale audio checks)
- `web_auth/` (Basic auth fuzz and per-request isolation for the async web servers, configs and runner, heap under paced
  connection churn with a TIME_WAIT runner, main task stack high-water mark under web_server load with `arena/` configs
  and runner)
- `web_handlers/` (late handler, web_server, captive_portal and prometheus variants, web OTA runner)
- `web_ota/` (damaged multipart upload sender with six modes, per case runner, captive portal on STA DUT)
- `web_server/` (SSE client, request mix, SSE and HTTPD case runners, per-session `/events` listing check with its
  config, `/events` head, multi-client, stall, Wi-Fi drop and churn modes with their config, head comparer and runner,
  `async_bump/` ESPAsyncWebServer bump flows and ESP8266 web OTA cases)
- `web_server/churnmix/` (`/events` close pattern churn with observers and REST, quiet rate run, HTTPS 501 check with a
  bench HTTPS component, summary)
- `web_server/json_ab.py` (byte-level A/B of every web_server JSON document, with its entity package)
- `wifi_channel/` (channel switching with a bench AP)
- `wifi_outage/` (test router and outage driver with a DHCP release hook)
- `wifi_restart/` (WPA3 mixed WISP upstream, static IP DUTs with syslog, runner and summary)
- `wifi_sniff/` (promiscuous per-destination frame counter component, sniffer config, timeline)
- `wisp_ap/` (WISP upstream with AP buttons, WISP and ALWAYS_ON DUTs, client, runner and per phase summary)
- `wpa3_ap/` (PMF AP with join and leave logging, BK7238 and CB3S DUTs, runner and summary)
- `wpa3_sniff/` (management frame sniffer AP and reboot stations)
- `xiaomi_ble/` (frame and header generator with the expected counts, injecting, advertising and receiving configs,
  counter and runner)
- `zephyr_ble_server/` (nRF52 server, looping ESP32 client and UF2 flash and count runner)
- `zephyr_coredump/` (reader flow, decode with the Zephyr tools)
- `zigbee_coord/` (ESP32-H2 Zigbee console coordinator: build, console bridge, driver and parser, network setup, device
  flash and capture, entity, number, copy and light flows with configs, per-build summary, endpoint dump and write
  injection header, leave debug wraps, S3 hold config for the Thread BR board)
- `zwave_proxy/` (checksum verifier, device info, `usb_unplug/` UART list and root port power cycle package)

## Templates in `$SKILL/templates`

- `dut-fallback-ap.yaml` (package): a device that never finds its network, falls back to its AP and
  serves the captive portal. `example-dut.yaml` is the per-run file with the platform block.
- `client-wifisave.yaml` (package): joins that AP, posts N bogus `/wifisave` requests, then the real
  credentials from `!secret`. `example-client.yaml` is the per-run file. `bogus_saves: "1"` gives a
  plain provisioning check.
- `diagnostics.yaml` (package): DEBUG logs, esphome OTA, uptime, free heap, reset reason and a 200 ms counter
  for soak and load tests.
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
  action package with per-board files and an OTA flash variant), `led_wire/` (DUT config and header that reads the
  LED strip pads back with RMT RX and an SPI slave), `uart_sim/` (simulator header, common package, per-build parts and run files) and `remote_base/` (common
  package, transmit script, per-board templates and the ESP8266 software sender), `i2c_fake/` (fake I2C device
  headers with their test configs, see `fake_i2c.md`), `network_dns/` (DNS loss diagnostics for
  `network: priority`, see `network_dns.md`).
- `microphone_http_stream/`: base config, fake counter and USB microphone variants and the multi-reader client,
  described in `reference/recipes/microphone_http_stream.md`.

## When the PR keeps moving

Record every head in the stack (`gh pr view N --json headRefOid`), build, and recheck before flashing; if anything
moved, wait five minutes and check again so the test runs on a settled head. When a head moved only by merging dev,
compare the PR's own diff (`git diff $(git merge-base OLD origin/dev) OLD` against the same for NEW); if it is
identical and the dev commits do not touch the code under test, earlier results still describe it, but rebuild so the
comment quotes the exact commits. Fetch `origin dev` before computing a merge base.

## When something looks wrong

- A harness header fails to compile on Arduino or LibreTiny: those frameworks define `millis()` and `micros()` as
  macros, so call `(esphome::millis)()`, which builds everywhere. Realtek SDK headers also define `OFF`, `ON`
  and `SUCCESS`.
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
- Copying a .uf2 to a mounted bootloader drive (`RPI-RP2`, `ITSY840BOOT`) from a terminal on macOS can fail with
  "Operation not permitted" (the Removable Volumes privacy permission), and picotool then sees no device. Flash through
  the serial path instead (`esphome upload --device <port>`: the 1200 baud touch on RP2, serial DFU on Adafruit nRF52
  boards) or by OTA; a board that only shows the drive needs the file dragged in Finder.
- A worktree shared with other sessions can change in the middle of a test: build PR images from your own detached
  worktree at the recorded commit (`git worktree add --detach <dir> <sha>`).
- A binary sensor's `on_state` does not fire for the first state unless the entry sets `trigger_on_initial_state:
  true`, so a log lambda on a sensor that stays on for the whole test prints nothing.
- `wc -l` pads its count on macOS, so `tail -n +$n0` needs `$((n0+0))` or an arithmetic expansion.
- `api:` reboots a board after 15 min without a client: `reboot_timeout: 0s` on `api:` and `mqtt:` for
  soak tests.

`reference/boards.md` has per-chip facts and crash decoding; `reference/recipes/` has the proven
flows. Add new findings there.
