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
   the shell that runs it. Never run `esphome logs` in a
   `while true` loop: the loop respawns it and holds the port.
2. **Credentials stay local.** Put a `secrets.yaml` next to the scratch configs and reference it with
   `!secret`. Never paste a config with credentials into a PR, never commit scratch configs or logs.
3. **Build from the tree under test.** `python -m esphome` puts the current directory first on
   `sys.path`, so run it with the checkout or worktree as the working directory and pass the yaml by
   its absolute path. A venv `esphome` entry point may import an installed snapshot instead of the
   checkout. Confirm with `python -c 'import esphome; print(esphome.__file__)'`, and for an A/B grep
   the build's `src/` for the change before trusting any number.
4. **PR build and baseline build get different `name:` values.** Builds share
   `.esphome/build/<name>`, so the same name means `esphome upload` can flash the wrong binary. Saved
   Wi-Fi credentials are keyed by the name too, so a test that must start unprovisioned gets a fresh
   name every run.
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
8. **Report** on the PR as a table: build SHA, board and framework, what was driven, counts, result,
   plus the `dev` control row and the exact flow in a `<details>` block. No credentials, no port names,
   no IP addresses.

## Flashing by chip family

| Board | Flash | Log |
|---|---|---|
| ESP8266 D1 mini (CH340) | `python -m esphome upload <yaml> --device <port>` | `serial_capture.py` at 115200; `esphome logs` on some CH340s emits garbage after the RTS reset. A failed esptool write can wedge the adapter until the board is replugged. `esptool erase_flash` before a test that needs an unprovisioned device: the preference sector survives a reflash |
| ESP32 devkit (CP2102 or CH340) | `esphome upload --device <port>`, ESP-IDF or Arduino | `serial_capture.py`. Default Wi-Fi light sleep delays API driven actions by seconds; judge timing from device logs. `power_save_mode: none` for OTA overlap tests |
| ESP32-S3, C3, C6, H2 native USB | `esphome upload --device /dev/cu.usbmodem*` | `logger: hardware_uart: USB_SERIAL_JTAG`. The USB console can wedge when the app takes over the console (0 bytes, esptool `No serial data received`); replug, or watch through `esphome logs --device <name>.local` |
| Pico W / Pico 2 W | `esphome upload --device <port>` (1200 baud touch); first flash ever via BOOTSEL and `--device /Volumes/RPI-RP2` or `RP2350` | port re-enumerates after upload: `ports.py --wait 2e8a` then capture |
| BW15 kit (RTL8720CF, rtl87xx) | `~/.platformio/penv/.libretiny/bin/python $SKILL/scripts/flash_rtl87xx_kit.py flash write -d <port> <build>/.pioenvs/<name>/*.uf2`. Plain `esphome upload` and plain ltchiptool time out because opening the port asserts DTR/RTS and holds the chip in reset | `serial_capture.py --released`, never `esphome logs` |
| BK7238 / CB3S (bk72xx, RTS on CEN) | `~/.platformio/penv/.libretiny/bin/python $SKILL/scripts/flash_bk72xx.py flash write -d <port> <uf2>` (about 75 s) | logs are on UART2 by default; `logger: hardware_uart: UART1` puts them on the flashing adapter. Opening the port resets the chip, so keep one capture open for the whole test |
| LN882H | ltchiptool forces 1 Mbaud for writes; weak adapters need `LN882H_YM_BAUDRATE` patched to 115200 in the penv's `ln882htool.py`; short BOOT to GND while the tool connects | OTA after the first flash |
| nRF52840 Dongle (PCA10059) | Nordic `nrfutil nrf5sdk-tools dfu usb-serial` on the bootloader port, or `dfu: true` in the config so the 1200 baud touch pin-resets into the bootloader; adafruit-nrfutil is refused | app has two CDC ports, ESPHome log on the second |
| Adafruit-style nRF52 (UF2 bootloader) | `UF2BOOT` volume or adafruit-nrfutil; `dfu: true` works through GPREGRET | single CDC port |
| Any reachable device | `esphome upload <yaml> --device <ip or name.local>`; `--file <bin>` for a prebuilt image | `esphome logs --device <ip>` bounded with `timeout` |

ESP-IDF OTA image: `.esphome/build/<name>/build/firmware.ota.bin`. LibreTiny OTA port is 8892.

## Scripts in `$SKILL/scripts`

- `ports.py`: list ports with VID:PID, product, serial and a board guess; `--wait VID[:PID]`,
  `--product NAME`, `--serial SN` (one specific board, whatever path it comes back under) and
  `--serial-of PORT` (the serial number behind a path).
- `serial_capture.py PORT SECONDS OUT [--baud] [--released] [--reset]`: bounded timestamped capture,
  ANSI stripped, reopens a vanished port, exits at the deadline.
- `flash_rtl87xx_kit.py`: ltchiptool wrapper for the BW15 kit whose CH340 drives CEN (RTS) and the boot
  strap (DTR). `flash read` needs the family `realtek-ambz2`; `flash write` does not.
- `flash_bk72xx.py`: ltchiptool wrapper for BK72xx boards whose RTS drives CEN.
- `wifisave_hammer.sh KIND DUT_PORT DUT_YAML DUT_TREE CLIENT_PORT CLIENT_YAML CLIENT_TREE [SETTLE]`:
  the fallback AP plus captive portal hammer end to end (compile, flash, capture, counts) with the
  templates below. KIND is `esp`, `pico`, `bw15` or `bk`. It ends with `RESULT PASS` (exit 0),
  `RESULT FAIL` (exit 3, the counts show a crash, an extra boot or no provisioning), `RESULT INVALID`
  (exit 2, a precondition was missed) or exits 1 when a build, flash or port step failed.

## Templates in `$SKILL/templates`

- `dut-fallback-ap.yaml` (package): a device that never finds its network, falls back to its AP and
  serves the captive portal. `example-dut.yaml` is the per-run file with the platform block.
- `client-wifisave.yaml` (package): joins that AP, posts N bogus `/wifisave` requests, then the real
  credentials from `!secret`. `example-client.yaml` is the per-run file. `bogus_saves: "1"` gives a
  plain provisioning check.

## When something looks wrong

- A capture stopped early or a line is missing: check the timestamps before trusting it.
- The client never joins the AP: another board may still run an AP with the same SSID, or the device
  booted with saved credentials because the name was reused. One AP device at a time on the bench.
- Builds come out byte-identical between branches: the wrong tree was imported (rule 3).
- Multicast from the host never arrives on some access points; test multicast device to device.
  Broadcast to boards is lossy under Wi-Fi power save, unicast is not. Throttle UDP floods to a fixed
  packet rate.
- `api:` reboots a board after 15 min without a client: `reboot_timeout: 0s` on `api:` and `mqtt:` for
  soak tests.

`reference/boards.md` has per-chip facts and crash decoding; `reference/recipes.md` has the proven
flows. Add new findings there.
