# modem

## PPPoS modem component with no modem attached (#6721)

A boot check on a classic ESP32 devkit with the modem UART pins left open. `templates/modem/modem-common.yaml` is
the package (`api:`, debug `loop_time` and `free` sensors, uptime, buttons for `modem.enable`, `modem.disable` and
`modem.send_at`, a 20 s alive log); `modemo.yaml` is the modem only build with `reboot_timeout: 2min` and the
component test config's `on_boot` block at `priority: -100` (earlier, `modem.send_at` runs before the modem's
`setup()` and logs `Modem DCE not ready`); `modemw.yaml` adds `wifi:` with `reboot_timeout: 0s` so the API stays
reachable. Expect `Modem not responding at 115200 baud` once per pass with each pass blocking the loop about 1 s,
the reboot at the configured timeout (`SW_CPU_RESET`), and a `modem.disable` with no modem answering to block
the loop about 37 s (`Failed to leave data mode`, then the radio off timeout) without tripping the 30 s watchdog
floor because the component feeds it in between. Drive the Wi-Fi build with
`scripts/modem/mdrive.py HOST LOGFILE list press:NAME wait:S` (plaintext API; prints sensor and text sensor
states, saves the device log). Before 8cc8df4a1f a modem only config with `api:` failed validation because a
config dependent `AUTO_LOAD` added `network` after the dependency check; an explicit `network:` block was the
workaround. Results: https://github.com/esphome/esphome/pull/6721#issuecomment-6347395159 and
https://github.com/esphome/esphome/pull/6721#issuecomment-6348520416

## SIM7080G on an M5Stack Atom Lite (#6721)

The M5 Unit CatM (SIM7080G) on the Atom Lite's Grove port: RX GPIO32, TX GPIO26, `model: SIM7080`.
`templates/modem/modem-passthrough.yaml` is a raw UART passthrough with both directions logged, and
`scripts/modem/mdrive.py HOST LOGFILE text:AT+CPIN? wait:5 ...` sends AT commands through it and saves the
replies, to check what the module does with no component in the way.

- With no SIM, the component sets the `SIM not ready` warning and polls `AT+CPIN?` once a second.
- With a SIM that has no data plan, `AT+CGATT=1` blocks the AT channel for about 141 s and then answers `ERROR`.
  The module answers nothing meanwhile, so the component logs `Network attachment request refused` and then `Modem not
  responding`, and loops every 2 minutes 22 seconds.
- `AT+CFUN=1,1` leaves the module silent for about 2.5 minutes. It comes back without a power cycle.
- A module left in CMUX (`AT+CMUX=0`, then an ESP restart) recovered 10 of 10 times (`Autodetect mode: 5`).
- PPP and a warm reboot during PPP need a SIM with a working data plan.

### SIM7080G with a live SIM

- A SIM whose plan does not cover LTE-M or NB-IoT never registers on the SIM7080G: CEREG goes 0 (not searching) right
  after CFUN=1, 2 (searching) for about 60 s, then 3 (denied) for a minute or two, then searching again, with CSQ 19 to
  31. COPS=? lists the Cat-M networks; manual COPS fails (ERROR or denied), so don't retry it. PPP, attach timing and a
  warm reboot during PPP need a SIM whose plan covers LTE-M.
- esp_modem 2.1.0 sends `AT+CGATT=1`, `AT+CGATT?` and `AT+CEREG?` with a 500 ms timeout. Unregistered, `CGATT=1`
  holds the AT channel (15.6 s denied in the passthrough, about 40 s in the component, over 2 minutes with a SIM that
  has no data plan) and the module drops every command sent meanwhile: no echo, no reply (an `AT+CMUX=0` sent then is
  lost).
- `ATD*99#` sometimes answers `CONNECT 150000000` without an attach. The module then sits in data mode, sends no LCP,
  ignores AT for minutes, and esp_modem's autodetect returns 0 forever (dev logs the wiring hint). A guarded `+++`
  (1.1 s silence, `+++`, 1.1 s) returns OK at once; give the passthrough an `Escape` template button doing
  `write_str("+++")` between two 1200 ms delays.
- `AT+CFUN=0` powers the SIM down only about 1.7 s after OK (`+CPIN: NOT READY` URC), and the component reads
  `+CPIN: READY` before that, so `init_at: [AT+CFUN=0]` cannot fake a missing SIM. There is no reliable no-SIM path
  without removing the SIM.
- `AT+CFUN=1,1` left the module silent for 20 s to 2.5 min.
- Pins that reach nothing, for the no-modem path on an M5Stack Atom (or any ESP32): `rx_pin: GPIO22`, `tx_pin: GPIO19`.
- Drive the component build with send_at buttons (`AT DIAL` = `ATD*99#`, `AT CFUN1`, `AT+CMGR=9` for an error text)
  and the `Restart` button; `mdrive.py press:NAME`. A `send_at` needs about 3 s before a restart to go out.

## SIM7670G on the Waveshare ESP32-S3-SIM7670G-4G (#22581)

- Board: ESP32-S3 console on a CH343 (UART0, so `logger: hardware_uart: UART0`; the default USB logger prints
  nothing). Modem UART: ESP TX GPIO18, ESP RX GPIO17, 115200, `model: SIM7670`. The modem is powered by the "4G" DIP
  switch, no GPIO power-up needed. Module SIM7670G-MNGV, firmware V1.9.05.
- The module's own USB enumerates as four `05c6:9330` ports; one of them is an AT port that works next to the
  component's UART (another is a binary diag stream). Send AT lines there to read CEREG, CGDCONT, CEER or SMS while the
  ESP runs. Run AT experiments with an idle ESP image (a hex passthrough), or the component's own reconnects (CFUN=4,
  CGDCONT) get mixed into the results.
- Hex passthrough with frame buttons: `uart: debug: after: timeout: 40ms` with `log_hex`, buttons that `uart.write`
  SABM/DISC/CLD/AT frames per DLCI (FCS: CRC-8 reversed poly 0xE0 over address, control, length, then 0xFF minus it)
  and buttons that write runs of 0xF9.
- Findings: echo is on after power-up or an AT channel reset, and esp_modem's mode guess takes the echoed LCP probe for
  a PPP reply (`Autodetect mode: 2`), the start of #22581. After an ESP reset during PPP the module stays in CMUX and
  ignores SABM/DISC/CLD/AT/+++ (it answers SABM0 with a UA only now and then); a burst of 4 KB of 0xF9 usually makes it
  print `QCRDY` and fall back to command mode with echo on (worked 10 times, failed 4 tries once after a reflash).
- A PDP context written at CFUN=4 makes the next attach fail with `+CEER: EMM_CAUSE_ESM_FAILURE` (CEREG 3) until a
  CFUN=0 cycle; written at CFUN=0 it attaches in about 8 s. After each attach the network can rewrite context 1's APN to
  its own name. An empty `apn: ""` validates and attaches (the network picks the APN).
- A SIM can attach and get an IP and DNS while no TCP or ICMP traffic comes back, through PPP (CMUX and data mode) and
  through the module's own `AT+HTTPACTION` (706) and `AT+CPING`: that is the carrier side, not the component.
- SMS: when the SIM's SM store is full, set the receive store to ME with `AT+CPMS="SM","SM","ME"`. Messages are UCS2
  hex in text mode; decode in memory only, never into a log, and keep SIM data (numbers, identifiers) out of results.

### Retest flow for the modem fixes (#22816, #22817)

- Drive config: Wi-Fi plus `api:`, a `Modem Connected` template binary sensor (`id(atmodem).is_connected()`) and
  template buttons (enable, disable, `send_at` for `ATE1`, `AT+CFUN=1`, `ATD*99#`, an error and a long reply,
  restart). Wait on the sensor from the API driver, but time each case from the serial log (reset to
  `WAIT_IP -> CONNECTED`), not from the driver's wait.
- Dev on the SIM7670G never reaches PPP after a warm reset that follows a PPP session: echo is back on after CMUX
  closes, so dev takes the false data mode path. Before a dev run, flash the fixed build, let it connect, press
  `Modem Disable`, then send `ATE0` through the passthrough, so both builds start from command mode with echo off.
- The stuck CMUX recovery needs the `+++` escape before the 4 KB flush, on every probe pass; flushing first, or skipping
  passes 2 and 3, recovered 0 of 6 times. Recovery takes about 15 to 20 s, or about 85 s.
- `AT+COPS=2` does not survive the component's CFUN cycle, so "not searching" cannot be forced on the SIM7670G.
- `modem.disable` after CMUX: `CFUN=0` times out on the SIM7670G ("Radio off failed"); `CFUN=4` works.
- Never send `AT+CPIN=` with arguments as a test command on a live SIM; `AT+CPBR=999` gives a safe `+CME ERROR`.
