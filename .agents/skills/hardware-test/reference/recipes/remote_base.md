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

## Climate IR loopback over the API (#22704 whirlpool, #22709 climate_ir)

- Pattern: one image with a sending climate and a receiving climate (`receiver_id`) per platform on one shared pin, an
  aioesphomeapi script that sends climate calls and reads every climate's state after each step. Several platforms can
  share one receiver; each rejects the others' frames. Scripts in `scripts/climate_ir_loop/`: `drive.py`
  (steps `Name:mode=cool,t=24,fan=low,swing=vertical,preset=eco` or `svc:action?flag=1`), `run.sh NAME PORT [STEPS]`
  with `steps.sh` (prelude and steps), `frames.py NAME` (splits the `dump: raw` lines per step), `compare.py A B [TOL]`
  (per step), `flatcmp.py A B TOL` (the whole sequence, timing by timing), `c-common.yaml` (Samsung, Friedrich,
  Siemens IRA211 and Mistral senders and receivers), `esp32-ir.yaml` / `esp8266-ir.yaml` (one-pin loopback),
  `c32pr.yaml` (an example run file) and `whirlpool-inject-common.yaml` (injected frame actions).
- ESP32-PICO-D4 kit: GPIO4 does not loop back cleanly (every mark reads 162 µs short, a -162 space leads the frame, so
  nothing decodes). GPIO27 gives exact timings (572/553/1659 µs). Try another pin before suspecting the code.
- RMT ends a frame on any level longer than `idle`, mark or space. A whirlpool 14 byte frame ends after a 7,960 µs gap,
  so its decoder needs the trailing idle to be within 25% of that, but the 9 ms header mark must stay below idle:
  `idle: 9500us` works, 8500 cuts the header off.
- Restored climate state changes the first frames (power toggles, Samsung power section): start every run with a
  prelude that puts each sender in a fixed state (every field each sender supports, such as humidity for daikin_arc
  or a fan mode daikin_brc supports), and compare only the frames after it. ESP8266 restore lives in RTC memory, so a
  receiver's state can carry over from the previous image.
- Inject frames a sender cannot produce (an out of range temperature code, a short frame) from an API action lambda with
  `item_bytes<BitTimings>` and `call.perform()`.
- Assigning frames to steps by host timestamps is fragile on the D1 mini (a log line can land after the next step
  starts); compare the whole frame sequence in order instead. Two frames sent back to back (siemens_ira211 power plus
  sync) come out as one dump, and the gap between them varies by up to 1 ms on ESP8266.
- Samsung power frames are 347 timings: an ESP32 receiver needs `rmt_symbols: 384` and `receive_symbols: 384`.

## Protocol A/B with `dump: all` (#22711)

- One button per `transmit_<protocol>` action, one binary sensor and one `on_<protocol>` trigger per protocol, each
  logging a marker; count markers and compare the dumper lines of the dev and PR runs in order. All of nec, dooya,
  nexus, aok and cardin_s438 decode in the ESP32 GPIO27 loopback with the default idle.
- Cardin S438 picks a random seed per frame (`random_uint32()`), so its pronto and cardin lines differ between any two
  runs; compare code and channel only. Each press sends 3 repeats, so its trigger and binary sensor fire 3 times.
- Start the dump extraction at the first `MARK tx` of any protocol: the API lists buttons by name, not config order.
- A Python-only change can still move generated code: diff `src/main.cpp` of the two builds (field order in action
  setters, defaults now written explicitly).

## Large climate frames in the loopback (#22710)

- At 115200 baud the serial log cannot keep up with long `dump: raw` frames (hitachi_ac344 is 691 timings, daikin
  sends 3 frames per command). Lines get lost or merged, frames show up under the wrong step, and the script reads the
  receiver state before it has decoded the frame. Use `logger: baud_rate: 921600`, capture with `--baud 921600`, and
  wait 4 s per step (`BAUD=921600 STEP_S=4 run.sh ...`). A CH340 D1 mini can garble 921600; 460800 carries the long
  frames cleanly there.
- On the classic ESP32 the receiver has no RX ping-pong, so a whole frame has to fit `rmt_symbols`. With a 64 symbol
  transmitter, `rmt_symbols: 448` and `receive_symbols: 448` hold hitachi_ac344.
- Receivers that keep state the frame does not carry (climate_ir_lg swing is a toggle, zhlt01 keeps its preset on
  "none") start each run from restored state, so compare them without those fields or erase the preferences first.
- Script pitfalls: in zsh `${BAUD:+--baud $BAUD}` is one word, so argparse rejects it. A stale capture file can feed
  the IP lookup and leave the run with no capture at all, so `run.sh` deletes the capture file first and kills
  whatever holds the port (`lsof -t`).

## Whirlpool A/B by hex lines (#22710 retest)

- whirlpool logs every frame at VERBOSE (`Sending:` and `Received:` hex), so on the D1 mini compare those bytes line by
  line instead of raw timings. One image per model (DG11J1-3A, HITACHI_HCRA31NEWH, HISENSE_ARG; "Hisense ARG" is a
  whirlpool model, not its own component). `scripts/climate_ir_loop/whirlpool/`: `wc.yaml` (common), `cl-*.yaml` (one
  per model), `steps.sh`, and `all.sh PORT BASE_TREE PR_TREE`, which builds and runs base then PR per model so the
  board ends on a PR build.
- The HISENSE_ARG receiver reports fan medium for fan_only while the sender says auto (dev too).

## Pronto dump A/B with `dump: all` (#22752)

- Scripts in `scripts/remote_base/pronto/`: `gen.py` writes `gen-common.yaml` (buttons for NEC and raw frames of
  21/41/43/121/245 timings, a soak button with heap logs, a `pronto:` binary sensor), `esp32-ir.yaml` (GPIO27) and
  `d1-ir.yaml` (GPIO12) include it, `run.sh NAME TREE YAML PORT BAUD ROUNDS` flashes, captures and runs `drive.py`,
  `extract.py` strips timestamps and source line numbers, `consist.py` checks every dump against the `on_pronto` text,
  `jit.py` counts frames that differ between two runs.
- The received frame ends with the idle gap as an extra timing (`0181` for the 10 ms idle), so a `pronto:` binary sensor
  built from the sent timings needs one more number at the end, or the length check rejects every frame. A 41 timing
  frame is then exactly 230 characters, one full log chunk: a good boundary case.
- ESP32 RMT loopback gives byte-identical Pronto text across builds. The D1 mini edge interrupt moves timings by one
  Pronto unit between any two runs (about half the frames), so compare D1 runs by dump against trigger consistency.
- `dump: all` prints NEC lines between the `on_pronto` trigger output and the Pronto dump; the API binary sensor
  ON/OFF pair can arrive collapsed, so count `on_press` log lines on serial instead.

## Protocol decode A/B on the host, climate fuzz, AEHA/Haier/Mirage loopback (#22773)

- `scripts/remote_base/hostharness/`: a `host:` config (`rbhost.yaml`) with a local `rbtest` component (AUTO_LOAD
  remote_base, `request_protocol()` for the protocols) and `rbh.hpp` called from `on_boot`; it encodes, mutates, decodes
  and dumps, then `exit(0)`. Run the `program` binary and compare stdout with timestamps and `:line` stripped. The Haier
  and Mirage decoders want one more item after the last mark (the receiver's idle gap): append `-20000` to transmitter
  timings or every clean frame fails.
- AEHA frames longer than 35 bytes decode as their first 35 bytes (the trailer mark equals a bit mark).
- `scripts/climate_ir_fuzz/`: copy `climate_fuzz.hpp` plus the matching `*_fuzz.cpp.example` (as `.cpp`) into
  `tests/components/<climate>/` of each tree (haier_tundra also needs `haier_init.py` as `__init__.py`), run
  `script/cpp_unit_test.py`, diff the `FUZZ` lines. All tests of one run share one main.cpp: use a macro include guard,
  not `#pragma once`, for a header copied to several test dirs.
- Lambda idiom checks: compile `void f(AEHAData x) { ... }` snippets with `-fsyntax-only -I<host build>/src` and
  `#include "esphome.h"` (format_hex lives in alloc_helpers.h).
- Loopback config `ir-common.yaml` (AEHA 2 to 35 bytes, Haier, Mirage from an API button, `press.py`). An ESP32-PICO-D4
  on GPIO27 never decodes 34 or 35 byte AEHA, even with `rmt_symbols: 384`, `buffer_size: 8kb` (dev too); a D1 mini on
  GPIO12 decodes all of them.

## Every dumper over a saved corpus, on the host (#22814)

`scripts/remote_base/dumpall/`: the `rbtest` component requests every `_PROTOCOL_STEMS` source; `rbh.hpp` needs a
`dumpers.h` generated from the `DECLARE_REMOTE_PROTOCOL(...)` lines of the tree (plus Pronto, RCSwitch, ABBWelcome;
Raw only when nothing matched). Encoders that randomise (Cardin S438 picks a random seed) make two builds see
different inputs: run once with `RBH_CORPUS=<file>` to write the corpus, then replay that file on both builds.
Merge back-to-back marks or spaces before decoding (Drayton never decodes otherwise). Nudging a quarter of all
timings to the tolerance edges proves nothing (the frame fails elsewhere): put only the sync mark at lo-1, lo, hi,
hi+1 and keep the rest exact; that variant caught a planted `< max` in 1,000 decodes per protocol.
`cmp.sh` compares the DEBUG and VERBOSE outputs (`rbhost-DEBUG.yaml`, `rbhost-VERBOSE.yaml`) with timestamps and line
numbers removed.
- Loopback with `dump: all` (`ir-common.yaml`, `drive.py`): Beo4 never decodes on one pin (needs its carrier) and
  Drayton's channel/command form has no checksum; compare decoded lines, raw and Pronto timings jitter between runs.
- `cc-common.yaml`: an ESP32-C3 with a CC1101 in async OOK at 433.92 MHz, GDO0 into remote_receiver, for the RF
  dumpers on real air (a quiet band gives only a few Pronto dumps a minute and no protocol hits).
