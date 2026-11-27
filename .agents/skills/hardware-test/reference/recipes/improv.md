# Improv serial and BLE (#22597)

`scripts/improv/` drives Improv on a freshly erased board with no saved network:

- `improvcli.py PORT OUT --secrets FILE`: Improv serial client. It shares the console with the logs and parses
  frames out of the log stream. The flow: 300 stray bytes with no `I` in them, state, info, network state, scan, a
  bad password (expect `unable_to_connect` after about 30 s), the real password, then state and network state
  again. It ends with a count of frames, bad frames, port resets, reboots and log lines. Credentials come from the
  secrets file and are never printed, and the SSID is masked in scan results. It reopens a native USB port that
  re-enumerates. A reboot is counted from each new `Running through setup()` log line after the settle, so a board
  behind a USB-UART bridge that reboots without a port reset is caught too.
  Every request must get its reply; a missing reply, an unexpected error, a malformed frame, a port reset or a
  reboot fails the run. `--skip-bad` leaves out the bad password case, `--keep-dtr` leaves DTR asserted,
  `--allow-reset` accepts a reboot.
- `improvble.py NAME SECRETS`: Improv BLE client (bleak). Finds the device by name, sends WIFI_SETTINGS and prints
  the state changes and the RPC result URLs. Needs `improv_ble` (formerly `esp32_improv`) with `authorizer: none`
  and a `next_url`; the result must carry a URL.
- `common.yaml`: package with Wi-Fi and no network, `web_server`, `improv_serial` with a `next_url` (so the
  provisioned reply carries URLs) and an ALIVE line every 5 s. `example-esp32.yaml` uses it on a classic ESP32.
- `run.sh NAME [improvcli args]` (env `TREE`, `PORT`, `SECRETS`, `PY`): compile, erase, flash and run the client.
  It erases with esptool, so ESP chips only; on LibreTiny or RP2040 erase and flash with that platform's tools and
  run `improvcli.py` directly. Both clients end with PASS or FAIL and exit non-zero on FAIL.

Per board:

- Improv serial uses the logger's UART, so the client must open the port the log is on. On LibreTiny check which
  UART `logger: hardware_uart: DEFAULT` resolves to (see the board notes).
- BW15: drop `web_server:` or add an `ap:` (it resets during setup otherwise), and pass `--allow-reset` for the known
  reboot after `Disabling AP`; the provisioned reply usually goes out first, not always. See `reference/boards.md`.
- ESP32-C3 on USB_SERIAL_JTAG: passes with and without `--keep-dtr`; its reboots only show in the
  `Running through setup()` count (see `reference/boards.md`). The scan returns only the last scan's results, which
  are empty in the first seconds after boot; use `--settle 10` or more when the target network should be listed.
- Pico 2 W: run with `--keep-dtr` (see `reference/boards.md`). Its Improv scan lists no networks, on dev and PR alike.
- Erase the flash between builds: a saved network skips the provisioning path entirely. Both clients fail at once
  when the board already reports `provisioned`; erase the flash first. A new config name does not help: the
  network is saved under a fixed key when the config has no `networks:`. A provisioned board also stops advertising
  Improv over BLE, so `improvble.py` reports the device as not found.
- If the real password fails right after the bad password case, rerun once before blaming the change: on the C3 it
  failed once (`Auth Leave` while Wi-Fi was still retrying the bad network) and passed on every rerun.

When posting results:

- Scan results list the neighbours' SSIDs; filter `RPC cmd=4 len=[1-9]` lines out of anything you post.
- Mask IP addresses in the result URLs.
- Cover a long password too: provision once against a network whose password is 20 characters or more (the client
  prints the length).

## Improv serial scan reply over the logger UART (#22758)

- `scripts/improv/improvscan.py PORT OUT [--settle S]` (on `improvcli.py`'s frame parser) sends an Improv RPC request
  scan (command 0x04) and records each reply frame (checksum, strings, auth field bytes and length) until the
  end-of-list reply. Opening the port resets a D1 mini and a classic ESP32 (see `reference/boards.md`), so it waits
  30 s before sending.
- A connected device frees its scan list, so the reply is just the end-of-list frame. Add a `wifi_info:`
  `scan_results:` text sensor to keep the list.
- For an auth `NO` entry you need an open network in range: run an AP-only build without a password on a second board
  (`scripts/improv/openap32.yaml`).
- Scan replies list the neighbours' SSIDs; filter them out of anything you post.

## USB Serial/JTAG RX ring heap check (#23171)

`scripts/improv/usj-heap-common.yaml` (with `usj-heap-c3.yaml` and `usj-heap-s3.yaml`) logs the free internal heap
and largest block from an `on_boot` priority 800 lambda and every 10 s, plus a 20 lines/s `SEQ` burst, with
`logger: hardware_uart: USB_SERIAL_JTAG` and no `improv_serial`. Reset from the host and capture each boot for about
36 s; compare the boot values of five boots per build.

- The `on_boot` priority 800 value is the same to the byte on every boot of one image (a second value 132 B lower
  shows up now and then on every build), far cleaner than a reading 30 s in. A 512 B to 65 B ring saved 444 B on the
  S3 (ESP-IDF rounds 65 up to 68) but 312 B on the C3, both the same on every boot.
- `usjstress.py PORT OUT SECONDS` writes text lines and random bytes into the console while capturing it; host
  writes into a console nobody reads (about 41 KB) never timed out. `burstchk.py RAW...` then checks every `SEQ`
  number per boot for gaps, duplicates and broken lines; it also serves the log burst check in the logger console
  stack section below.
- Opening a C3 or S3 USB Serial/JTAG port with DTR and RTS low resets the chip once (`rst:0x15
  USB_UART_CHIP_RESET`); start host writes 10 s after opening.

## Logger console stack (#22596, #22597)

- Log burst during Improv: an interval that logs 10 numbered `SEQ <n>` lines plus 10 lines without arguments per
  second. Check the raw capture for gaps, duplicates and broken lines, ignoring a partial first or last line.
- Console echo without Improv: `esphome: platformio_options: build_flags: [-DUSE_LOGGER_CONSOLE]` plus a 20 ms
  interval calling `logger::global_logger->console_read()` and `console_write()`. Host writes of up to 300 bytes echo
  exactly on ESP32 UART0 (ESP-IDF and Arduino), ESP8266 UART0, C3 and S3 USB_SERIAL_JTAG and RP2040 USB_CDC. Only
  the S3 USB_CDC (OTG) console keeps just the first 91 bytes of one larger write (the ESP-IDF RX buffer).
- ESP32-S3 `hardware_uart: USB_CDC` with `improv_serial` is refused by config validation; use USB_SERIAL_JTAG.
- ESP32-C3 `hardware_uart: USB_CDC` with Improv: the device never reads its USB Serial/JTAG port, so a host
  `flush()` (tcdrain) blocks forever. Send requests without a flush and with a `write_timeout`.
- A right password that fails right after the wrong password case is a Wi-Fi retry race (the connect fails at once
  after the new settings and RETRY_HIDDEN does not resolve within 30 s); rerun before blaming the change. For a clean
  start on every run, reflash and call `global_preferences->reset()` from an `on_boot` priority 800 lambda.
- The device logs the Improv password at DEBUG; keep raw captures local.

## Log macro rewrites, A/B by log text (#22758)

`scripts/log_macros/`: `lm-common.yaml` (one entity of every common platform plus `improv_serial` and `web_server`),
`drive.py HOST OUT` (presses the exercise button, reads API sensor states and web_server JSON) and `norm.py SERIAL`
(keeps config dump lines, `lm` lines and state lines, and replaces addresses, the device name, signal, setup times
and per-boot values with placeholders instead of dropping the lines) so two builds' captures diff cleanly. Recent ESPHome logs sensor states and switch, light and fan calls only at VERBOSE; at
DEBUG a log diff sees none of them, so `lm-common.yaml` logs at VERBOSE.
