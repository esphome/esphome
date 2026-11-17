# Tuya MCU simulator (#22724)

No Tuya device needed. `scripts/tuya_sim/`: `tuyasim.hpp` is a Tuya MCU on a second UART of the same ESP32, cross-wired
through the GPIO matrix to the hub's UART (hub tx 9 rx 10, MCU tx 10 rx 9 on an ESP32-PICO-D4, `board: pico32`,
`allow_other_uses: true` on all four pins, 9600 baud). It answers heartbeat (0x00 first, then 0x01), product query,
conf query with an empty payload (so the hub takes the WIFI_STATE path), wifi state and datapoint query (one report with
all 14 datapoints), echoes each DATAPOINT_DELIVER back as a report like a real MCU, and logs every hub frame as
`SIM RX #n cmd XX: <hex>`. `tuyasim::start()` (the "Sim start" button) plays 18 scripted steps 1.5 s apart: every
datapoint type (bool, value incl. negative, string, enum, 1/2/4 byte bitmask, raw), a four datapoint frame, one
DATAPOINT_REPORT_SYNC (the hub must ack with 0x23), a repeated value.

- `tuya.yaml`: hub plus binary_sensor, switch, three sensors (int, enum, bitmask), three text sensors (string, enum,
  raw), three selects (enum, string, int), water heater (switch/target/current/mode), number and fan. Hub
  `on_datapoint_update` lambdas log `HUB dpN x=.. <entity>=..` with the entity's state, and entity triggers log
  `ENT <id> <value>`, so the hub-before-entity order and the old state the hub sees are visible per datapoint.
  `name: ${name}`, built with `pio_build.sh compile <name>` per tree.
- `tuyadrive.py HOST OUT.json` (API_KEY env): subscribes states, presses Sim start, then sends switch, select,
  water heater (on, target, mode eco/electric, off), number and fan commands 2 s apart; prints per phase counts.
- `run.sh NAME TREE` (env PORT, API_KEY): upload, 130 s serial capture with a reset, drive.
- `compare.sh A B`: diffs API states, the entity dump block, the 1 s re-dump, the HUB/ENT order and the non-heartbeat
  frames the simulator received. Heartbeats move with timing, so they are left out.

Facts from the run:
- At init the fan delivers its own state (`1f01000100`) before DP31 is known and the hub logs `Setting unknown
  datapoint 31`; same on dev.
- The API coalesces states published in one loop pass: the four datapoint water heater frame gives one state.
- A switch or select command for the value it already has sends no frame.
- Water heater target 55 with multiplier 0.5 is sent as 110 (`15 02 0004 0000006e`); eco/electric as enum 0/2 on DP23.
- Generate an `api_key` for `secrets.yaml` with `openssl rand -base64 32` if you have none.
- #22724 changed only the dump order (children now print right after the hub's own lines); `setup_priority:` on a
  tuya sensor is rejected on the PR and accepted on dev.
