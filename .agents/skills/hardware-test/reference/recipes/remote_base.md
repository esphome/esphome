# remote_base / remote_receiver

## Loopback on one pin, no IR hardware (#21560 to #21563)

`templates/remote_base/`: `rb-common.yaml` (API action `tx_all`, a dumper registered from `on_boot` through
`register_dumper()` standing in for an external component), `rb-txscript.yaml` (14 `remote_transmitter.transmit_*`
frames with `tx <proto>` log markers), `rb32.tmpl` and `rb66.tmpl` (`@V@` is the build label).
`scripts/remote_base/run.sh LABEL B N` (`TREE`, `P32` or `P66`, `SC` in the environment) writes the run file from the
template, builds and flashes it from `TREE` (`esphome run --no-logs`), finds the IP and runs `rbdrive.py IP N`, which segments the API log by the
markers and prints what each sent frame decoded as.

- ESP32: `remote_transmitter` and two `remote_receiver`s on the same GPIO (`allow_other_uses`), carrier 100%, no
  inversion. RMT reads its own output, every frame decodes. Second receiver `dump: [nec, raw]` shows raw runs only
  where no primary matched.
- ESP8266 before #22453: the transmitter sends the whole frame with interrupts off, so a receiver on the same pin
  sees nothing (from #22453 on it decodes, see below). For older trees, `softtx.hpp`
  bit-bangs frames built with each protocol's `encode()` with interrupts on, and an `interval` sets the pin back to
  OUTPUT after the receiver's setup. Bit-banged NEC decodes as NEC about 2 of 3 times (timing), on every build.
- Dish never decodes in loopback on dev either. Pronto matches every frame, so raw on `dump: all` never prints.
- API log lines can drop under bursts; rerun with more rounds before calling a shortfall.
- `register_dumper()` returns nothing, so the config logs `ext dumper register called`, not whether it stuck. On dev
  the list has a fixed size and a dumper beyond it is dropped with `No dumper slot` (ERROR); judge by that line and by
  `ext dumper saw` counts in the `rbdrive.py` table, which only appear when the dumper is really registered.
  `rbdrive.py` exits 1 when the external dumper saw no frame, so a build that rejects the registration fails the
  drive; set `EXPECT_REGISTER=1` on `run.sh` to also fail the run on any `No dumper slot` line.

Results: https://github.com/esphome/esphome/pull/21563#issuecomment-6381304387

Retest of the moved stack with 5 rounds per build: https://github.com/esphome/esphome/pull/21563#issuecomment-6382075726.
A D1 mini CH340 sometimes fails the 460800 baud upload; esphome retries at 115200 by itself, but a runner that reads
only the last line sees "Failed to upload" first, so rerun before blaming the build.

## ESP8266 transmitter to receiver on the same board (#22453)

- Wire the `remote_transmitter` pin to a `remote_receiver` pin (GPIO12 to GPIO14 on a D1 mini), `inverted: false`.
- Keep the receiver's default `idle`. `idle: 4ms` is shorter than the NEC leader space (4.5 ms), so every frame is
  split and nothing decodes.
- Dev before #22453 sends the whole frame inside one `InterruptLock`, so a receiver on the same chip decodes 0
  frames. That is expected on old builds, not a wiring fault. With locks per edge, 3,692 of 3,695 frames decoded
  under Wi-Fi load.
- Count with `on_nec` matching a counter in the command, and log the counts on an `interval`.
- Without a jumper, put both on GPIO12 with `allow_other_uses: true` and call `pinMode(12, OUTPUT)` before each frame
  (the receiver's setup leaves the shared pin as an input); the receiver's edge interrupt sees the transmitter's own
  output. `scripts/esp8266_lock_load/ir-one-pin.yaml` does this, and its `run.sh` adds Wi-Fi load (see
  [one_wire.md](one_wire.md)). Under load, the build that sends the whole frame in one lock hit a Hardware Watchdog in
  `wDev_ProcessFiq` after 11 minutes.
