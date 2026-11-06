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
- ESP8266: the transmitter sends with interrupts off, so a receiver on the same pin sees nothing. `softtx.hpp`
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

## BK72xx IR receive without a receiver: drive the pad from the chip (#21889, beken_irda)

`templates/beken_irda/` (configs and `irsim.hpp`) and `scripts/beken_irda/` (runners and `ana.py`). `irsim.hpp`
turns the receive pin's output driver on (GPIO config register `0x802800 + 4*pin`: bit 2 input enable, bit 3 output
enable active low, bit 1 level) while the input stays on, so the timer interrupt (or a GPIO interrupt) reads the
level the test drives. Receiver shape: idle high, low on a mark. `ir-common.yaml` (BK7238 board `xh-wb3s`) and
`cb-common.yaml` (BK7231N board `cb3s`, logs over the API) run the cases: NEC, repeats, other protocols, glitches,
a long frame, 200 random frames. `irt`/`cbt` are timer mode, `irh`/`cbh` hardware mode, `irf` timer mode with
custom filter settings, `irr` the stock `remote_receiver` on the same pad, `irp`/`cbp` register probes (`probe.hpp`).

- Suspend the scheduler (`vTaskSuspendAll`) around each frame. Without it, Wi-Fi and other tasks stretched about 40%
  of the marks by 0.3 to 1 ms, and only about 60% of the NEC frames decoded. Interrupts stay on, so the sampler
  still runs.
- Runners (run them in a copy of `templates/beken_irda`; `TREE` and `PY` in the environment): `run.sh NAME` builds
  and flashes over serial with `flash_bk72xx.py` (`PORT`, `SC`, `BKPY`, `BKFLASH`); it can fail several times in a
  row and then work, so after the first flash use `runota.sh NAME IP` (OTA, then the serial log). `runnet.sh NAME IP`
  is for a board with no serial access: OTA and the log over the API. All three end with `ana.py LOG`, which pairs
  each `TX` line with the `RX` lines that follow it, prints the score per case and fails without `CASE done`.
- BK7238 results: timer mode decoded 100%. Hardware mode fails in setup: the BK7238 has no legacy IRDA block
  (`irp.yaml` shows 0x802400 reads back 0 after any write, even with the IRDA clock powered and ungated, and P26
  never gets its second function). `remote_receiver` on LibreTiny decoded 0% (it loses about half the edges).
- The BK7231N has the IRDA block (control `0x02392101`), but its decoder never sees a level the chip drives on its
  own pin, with the second function on or off. Hardware mode needs an external signal (another board's transmitter
  wired to P26).
- Timer mode loses about 1 frame in 200 on both chips: one duration off by 250 µs or more, ticks lost while
  interrupts are held off.
