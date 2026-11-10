# Simulated UART sensors on a classic ESP32 (#21449, #21446)

No sensor needed. `templates/uart_sim/sim.hpp` (included with `esphome: includes:`) has frame builders for wts01,
ds1603l, sm300d2 and ufm01, `sim::loopback(id(uart))` (ESP-IDF `uart_set_loop_back` on the component's own UART),
`sim::send()` that logs the bytes, and `sim::probe`, a SEN0465 responder polled from a 10 ms interval. The run files
`fra.yaml`, `frb.yaml` and `frc.yaml` set the name and include `fr-common.yaml` plus their `fr*-parts.yaml` as
packages; copy them with a different name (`frabase` and so on) for the base build.

- Push-style sensors: put the component on a UART, switch it to loopback in `on_boot`, and inject frames from
  template buttons (`fra-parts.yaml`, `frb-parts.yaml`). ufm01 enters its active stream by itself when a 1 s
  interval streams frames during its 2 s startup wait.
- Request/response sensors: cross-wire two UARTs through the GPIO matrix (UART1 tx 25 rx 26, UART2 tx 26 rx 25,
  `allow_other_uses: true` on all four pins); the second UART runs the simulated device (`frc-parts.yaml`).
  A select sets the probe's misbehaviour (tail, FF tail, silent, split, wrong gas).
- Polled readers: run `update()` from a script and log `micros()` around it to measure blocking
  (sm300d2 on dev blocks about 104 ms on an empty buffer).
- The logger stays on UART0, so three builds cover five components. Drive with `scripts/uart_sim/frdrive.py HOST
  LOG press:NAME sel:NAME=OPT num:NAME=V wait:S @FILE` (`@FILE` reads one op per line), or
  `scripts/uart_sim/run.sh NAME TREE SECONDS OPS...` (build and flash from TREE with `esphome run --no-logs`,
  capture, drive; `PORT` and `SC` in the environment, `ERASE=1` erases the flash first, `DRIVER` picks another
  API driver).

## Particulate sensors: gcja5, pm1006, sds011 (#21939)

`templates/uart_sim/pmsim.hpp` adds frame builders `sim::gcja5`, `sim::pm1006` and `sim::sds011`, each with a bad
checksum or bad end flag. `fp-parts.yaml` puts gcja5 on UART1 (9600 8E1, with a template binary sensor that shows
`status_has_warning()`) and pm1006 on UART2; `fs-parts.yaml` has sds011 alone. Write the run files the same way as
`fra.yaml` (`fp.yaml`, `fs.yaml` with their own name).

- Every button injects one case with its own PM value, so each published state names its case: good, two frames,
  bad checksum, end or tail, garbage before a frame, a cut-off frame followed by a good one, a repeated first header
  byte, and splits of 300 and 700 ms (either side of the 500 ms gap reset).
- `run.sh NAME TREE 150 @fp-ops.txt` runs the op list from the file; `fs-ops.txt`, `fd-ops.txt` and `fj-ops.txt`
  are the other lists.
- The loopback also feeds each component its own writes (pm1006 request, sds011 setup and sleep/work commands),
  which must not disturb the next frame.
- The API coalesces two states published in the same loop pass, so "two frames" shows only the second value.
- Found with it: on dev gcja5 published bad and misaligned frames as readings (268435456).

## 4-byte distance sensors: a01nyub, a02yyuw, jsn_sr04t (#21948)

`sim::ds1603l(mm, bad_ck)` builds the same `FF H L sum` frame; `fd-parts.yaml` (a01nyub on UART1, a02yyuw on UART2) and `fj-parts.yaml`
(jsn_sr04t, polled every 15 s), with the same case list as above plus a reading below the minimum distance. With
loopback the jsn_sr04t request byte `55` can land inside a split frame and break it; check the `Request read out`
timestamps before calling a difference between builds.

## LD6002B TinyFrame parser (#19836, #19837)

`templates/uart_sim/ld6002b/`: `ld6sim.hpp` is a fake module on a UART cross-wired to the component's UART (UART1
tx 25 rx 26, UART2 tx 26 rx 25), set up by the shared `ld6-base.yaml` package. It ACKs every command with an empty
frame of the same type and logs the bytes it received (`MOD RX`), so the bytes on the wire can be diffed between
builds. Target reports carry a distinct X per
case, and `T1 X` shows which frames got through.

- `ld6-parser-cases.yaml` (used by `ld6-common.yaml`, which turns the query answers off): good, byte by byte (one
  byte per poll, so the component's loop runs in between), garbage before a frame, bad and lost header or data
  checksums, an oversized payload, a burst of 20. The script runs by itself from `on_boot`; `ld6p.yaml` is the run file
  (copy it with another name for the base build). `run.sh NAME TREE` builds, flashes over serial and captures,
  and fails without a `CASE done` line.
- Garbage that contains `01` right before a frame loses that frame on dev and on the PR (a false header, no rescan).
- Full component A/B: `ld6sim.hpp` also answers every query with a fixed report (delay, sensitivity, trigger,
  installation, Z range, low power, low power sleep, both area reports, version); `ld6x-common.yaml` has the full
  entity set and runs the parser cases too (`ld6x.yaml` is its run file); `ld6drive.py HOST` presses every button,
  sets every number and select and toggles every switch over the API. Run it with `ERASE=1 DRIVER=ld6drive.py
  scripts/uart_sim/run.sh NAME TREE 100`: switch and select states and the area id are stored preferences, and a previous build's run changes the
  next build's boot state.
- `m4-common.yaml` and `m5-common.yaml` run the same fake against `ld6004`: target and area reports, work status and
  radar on/off reports, malformed reports, and the point cloud and target display switches across a reboot
  (`m5` reboots once and checks the restored switch states). `m4.yaml` and `m5.yaml` are the run files, and like
  `ld6p.yaml` each can be copied with another name for the base build. Run m5 with `ERASE=1 run.sh NAME TREE 75`: the boot
  counter and switch states are stored preferences that survive a flash, and the second boot's `CASE done` comes
  about 49 s after reset. Only the second boot logs `CASE done`, so a skipped reboot fails the run.

See `rs485_frame.md` for the same cross-wired UART trick on an ESP32-C3.
