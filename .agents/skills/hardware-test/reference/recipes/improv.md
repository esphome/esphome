# Improv serial and BLE (#22597)

`scripts/improv/` drives Improv on a freshly erased board with no saved network:

- `improvcli.py PORT OUT --secrets FILE`: Improv serial client. It shares the console with the logs and parses
  frames out of the log stream. The flow: 300 stray bytes with no `I` in them, state, info, network state, scan, a
  bad password (expect `unable_to_connect` after about 30 s), the real password, then state and network state
  again. It ends with a count of frames, bad checksums and log lines. Credentials come from the secrets file and
  are never printed, and the SSID is masked in scan results. It reopens a native USB port that re-enumerates.
  Every request must get its reply; a missing reply, an unexpected error, a bad checksum or a port reset fails the
  run. `--skip-bad` leaves out the bad password case, `--keep-dtr` leaves DTR asserted, `--allow-reset` accepts a
  reboot.
- `improvble.py NAME SECRETS`: Improv BLE client (bleak). Finds the device by name, sends WIFI_SETTINGS and prints
  the state changes and the RPC result URLs. Needs `esp32_improv: authorizer: none`.
- `common.yaml`: package with Wi-Fi and no network, `web_server`, `improv_serial` with a `next_url` (so the
  provisioned reply carries URLs) and an ALIVE line every 5 s. `example-esp32.yaml` uses it on a classic ESP32.
- `run.sh NAME [improvcli args]` (env `TREE`, `PORT`, `SECRETS`, `PY`): compile, erase, flash and run the client.
  It erases with esptool, so ESP chips only; on LibreTiny or RP2040 erase and flash with that platform's tools and
  run `improvcli.py` directly. Both clients end with PASS or FAIL and exit non-zero on FAIL.

Per board:

- Improv serial uses the logger's UART, so the client must open the port the log is on. On LibreTiny check which
  UART `logger: hardware_uart: DEFAULT` resolves to (see the board notes).
- BW15 (and other LibreTiny boards with no Wi-Fi network and no `ap:`): `web_server:` makes the board reset about
  10 s into setup, on dev too. Drop `web_server:` or add an `ap:`. With an `ap:` every provisioning reboots once
  after `Disabling AP` (the known AP to station fault); the provisioned reply usually goes out first, not always.
  The kit's port does not re-enumerate, so the client sees no port reset; on native USB boards use `--allow-reset`
  only for a reboot that is known and expected.
- Pico 2 W: its USB CDC sends nothing until the host asserts DTR, so run with `--keep-dtr`. Its Improv scan lists
  no networks, on dev and PR alike.
- Erase the flash between builds: a saved network skips the provisioning path entirely.

When posting results:

- Scan results list the neighbours' SSIDs; filter `RPC cmd=4 len=[1-9]` lines out of anything you post.
- Mask IP addresses in the result URLs.
- Cover a long password too: provision once against a network whose password is 20 characters or more (the client
  prints the length).
