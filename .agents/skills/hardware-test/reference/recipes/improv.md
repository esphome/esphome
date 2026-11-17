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
