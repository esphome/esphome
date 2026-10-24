# remote_base / remote_receiver

## Loopback on one pin, no IR hardware (#21560 to #21563)

`templates/remote_base/`: `rb-common.yaml` (API action `tx_all`, a dumper registered from `on_boot` through
`register_dumper()` standing in for an external component), `rb-txscript.yaml` (14 `remote_transmitter.transmit_*`
frames with `tx <proto>` log markers), `rb32.tmpl` and `rb66.tmpl` (`@V@` is the build label).
`scripts/remote_base/run.sh LABEL B N` (`TREE`, `P32` or `P66`, `SC` in the environment) writes the run file from the
template, flashes it, finds the IP and runs `rbdrive.py IP N`, which segments the API log by the
markers and prints what each sent frame decoded as.

- ESP32: `remote_transmitter` and two `remote_receiver`s on the same GPIO (`allow_other_uses`), carrier 100%, no
  inversion. RMT reads its own output, every frame decodes. Second receiver `dump: [nec, raw]` shows raw runs only
  where no primary matched.
- ESP8266: the transmitter sends with interrupts off, so a receiver on the same pin sees nothing. `softtx.hpp`
  bit-bangs frames built with each protocol's `encode()` with interrupts on, and an `interval` sets the pin back to
  OUTPUT after the receiver's setup. Bit-banged NEC decodes as NEC about 2 of 3 times (timing), on every build.
- Dish never decodes in loopback on dev either. Pronto matches every frame, so raw on `dump: all` never prints.
- API log lines can drop under bursts; rerun with more rounds before calling a shortfall.
- `register_dumper()` returns nothing, so the config logs `ext dumper register called`, not whether it stuck. On dev
  the list has a fixed size and a dumper beyond it is dropped with `No dumper slot` (ERROR); judge by that line and by
  `ext dumper saw` counts in the `rbdrive.py` table, which only appear when the dumper is really registered.

Results: https://github.com/esphome/esphome/pull/21563#issuecomment-6381304387
