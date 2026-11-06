# Daikin Madoka BRC1H over BLE: protocol notes and a fake remote (#5951)

## Protocol

Sources: pymadoka (github.com/mduran80/pymadoka), the openHAB binding `org.openhab.binding.bluetooth.daikinmadoka`
(MadokaMessage.java and its tests), and the reverse-engineering notes at github.com/blafois/Daikin-Madoka-BRC1H-BLE-Reverse.

- Service `2141e110-213a-11e6-b67b-9e71128cae77`. Notify characteristic `…e111` (device to central),
  write-without-response characteristic `…e112`. Pairing uses numeric comparison with MITM, confirmed on the BRC1H.
  The device only answers; it sends nothing unprompted, and nothing at all until notifications are enabled.
- Chunks of at most 20 bytes: a chunk id (0, 1, ...) followed by up to 19 message bytes. Notifications can arrive
  out of order.
- Message: `[len][0x00][cmd_hi][cmd_lo]` followed by records `[id][size][value…]`. `len` counts the whole message,
  including itself. A query with no arguments sends `00 00`. Example request: `00 07 00 40 30 20 01 01` = chunk 0,
  len 7, SetOperationMode, arg 0x20 = 1 (DRY).
- Size byte `0xFF` means an empty value: both pymadoka and openHAB parse it as size 0 (GetOperationHours replies
  contain it).
- Commands (get / set):
  - 0x0020 / 0x4020 power, arg 0x20 = 1 or 0
  - 0x0030 / 0x4030 mode, 0x20 = 0 fan, 1 dry, 2 auto, 3 cool, 4 heat, 5 ventilation; the reply also has 0x1F
  - 0x0040 / 0x4040 setpoint: 0x20 cooling, 0x21 heating, 2 bytes big endian in 1/128 °C. The reply also carries
    limit records 0x30 to 0x32, 0xA0 to 0xA5 and 0xB0 to 0xB5 (1 or 2 bytes each)
  - 0x0050 / 0x4050 fan: 0x20 cooling, 0x21 heating; 1 low, 2 to 4 medium, 5 high, 0 auto
  - 0x0110 sensor information: 0x40 indoor (1 byte, °C), 0x41 outdoor (openHAB reads the first byte; 0xFF = not
    available)
  - 0x0100 clean filter indicator (0x62); 0x4220 disables it; 0x0130 maintenance and versions; 0x0302 / 0x4302 eye
    brightness (0x33, 0 to 19); 0x0000 general info
- Both reference clients send one command and wait for its reply before sending the next.

## Fake BRC1H

Two boards: `templates/daikin_madoka/madokafake.yaml` (the fake, with the `ext/madoka_fake` local component) and
`madokacli.yaml` (the `daikin_madoka` climate under test). Drivers are in `scripts/daikin_madoka/`.

- The fake is a `Component` that reassembles chunks from the `e112` `on_write` (`id(fake).on_chunk(x);`), answers
  the GETs above plus the SETs (`00 00` reply), keeps state and logs `STATE power= mode= cool= heat= fan_cool=
  fan_heat=`, and sends through `set_value(...)` + `notify()` in 19-byte chunks. The setpoint reply carries the limit
  records, so it is 61 bytes in 4 chunks, which is what makes the out-of-order fault meaningful. The notify
  characteristic is wired in from an `on_boot` (priority -100) lambda, `id(fake).set_chars(id(madoka_nfy));`.
- Faults come from a select: none, ff_record (a `0x1E,0xFF` record first in each GET reply), out_of_order (chunks 0
  and 1 swapped), drop_sensor, drop_setpoint, delay_100, delay_300, slow_1s, tail_late (chunks 2 and up of a
  multi-chunk reply go out 120 ms late; replies ready in the meantime log `HOLD` and wait behind the tail, as a
  device sends one message at a time). With the client's 50 ms GET spacing, tail_late on the setpoint reply lands
  after the fan and sensor GETs went out.
- Pairing: a custom component only gets GAP events by asking for a slot: `esp32_ble.register_gap_event_handler(parent,
  var)` with `cv.GenerateID(esp32_ble.CONF_BLE_ID)` in the schema. Answer SEC_REQ with `esp_ble_gap_security_rsp(true)`
  and NC_REQ with `esp_ble_confirm_reply(true)`. Both sides use `io_capability: display_yes_no`; the fake adds
  `auth_req_mode: sc_mitm_bond`. Success logs `auth success mode = 13`. The bond survives reflashing the client
  without an erase.
- The client needs the fake's Bluetooth MAC, which is not in the device info: it is the base MAC + 2 (Wi-Fi STA MAC
  + 2). Pass it with `esphome -s fake_mac <mac> run madokacli.yaml`.
- The fake was first run on an ESP32-S3 with W5500 Ethernet; the template uses a classic ESP32 on Wi-Fi, like the
  client.
- Client logging: VERY_VERBOSE (or VERBOSE on `ble_device_base` / `esp32_ble_tracker`) blocks the loop on scan
  results and trips the task watchdog. Keep those two at INFO. A 460800 logger baud rate gave no output on a classic
  ESP32 devkit; stay at 115200.
- `esphome upload` flashes the last build without recompiling. After a YAML change use `esphome run --no-logs`.

Drivers (all take the client and fake IPs and need `aioesphomeapi`):

- `drive.py CLIENT_IP FAKE_IP [faults|control|midbatch]...`: faults sets each fault, a unique setpoint and
  alternating COOL/HEAT through the client, waits two polls and prints the client state next to the fake's STATE
  line and the publish count in the window. control runs modes, setpoints with clamping and fan speeds. midbatch
  fires a setpoint SET from the fake's log stream the moment a chosen GET (or a TAIL) arrives, prints the fake's
  RX/TX order around it and exits non-zero when a SET did not read back.
- `setpoint_repeat.py CLIENT_IP FAKE_IP [x]`: repeated setpoint pair changes with a timeline; `x` puts a mode change
  before each one. Exits non-zero on a mismatch.
- `clistate.py HOST [SECS]`: prints the climate state as published.

Results on #5951: pairing, polling, control and delays passed. An early head published nothing under drop_sensor
(it published only in the 0x0110 case), and ff_record and out_of_order lost the reply (`find_arg` stopped at size
0xFF; the assembler reset on an unexpected chunk id); the retest passed all faults including tail_late and SET
mid-batch (the SET queues behind the batch's GETs). A command can take seconds (once about 13 s) to reach the client
over Wi-Fi while BLE is busy; check when `control()` ran (an off-grid "Got update request" line in the client log)
before calling a stale readback a defect.
