# rs485_frame DLE/STX/ETX bus without a bus (#16758)

`scripts/rs485_frame`: an ESP32-C3 (logger on USB_SERIAL_JTAG) with the hub's UART on tx GPIO3 rx GPIO4 and a
simulated bus device on tx GPIO4 rx GPIO3, cross-wired through the GPIO matrix (`allow_other_uses: true` on all
four pins), 9600 baud, no wires. The simulator is polled from an `interval` and logs with the `sim` tag; the test
script logs with `hwt`.

- `simrs.hpp` builds sum16 big endian, header-inclusive frames with `10 00` stuffing and logs what the hub sends.
  `common.yaml` runs good / bad CRC / garbage / split at 25 and 150 ms / overlong / max length / two frames in one
  write / a cut frame followed by a new DLE STX / a bad escape, then button presses that the hub must hold until a
  `01 01` gate frame arrives (`replace_latest` keeps only the last press).
- `simrs2.hpp` logs each frame the hub sends with start and end poll times, the silence since the previous frame
  and the time since the simulator's own last byte. `simrs::double_mode` switches the stuffing to `10 10`.
  Case files: `rsi2.yaml` (idle_gap gate), `rsx.yaml` (fixed_delay gate), `rsd.yaml` (sniffer_only, double escape).
- `run.sh CASES LABEL [SECONDS]` (`CASES` is `common`, `rsi2`, `rsx` or `rsd`; `TREE`, `PORT`, `SC` in the
  environment) writes a run file with its own name, builds, flashes, captures and writes `out-rs<CASES><LABEL>.log`
  with time stamps and line numbers stripped. It fails without a `DONE` line or on a crash. Diff the logs of dev and
  the PR; for the gate timing cases read the `RXF` lines (silence before each frame the hub sent).
- On ESP32 at 9600 baud the default `rx_full_threshold` is 8, so a component does not see bus bytes for about 9 ms.
  Queue a test frame at least 20 ms after the bus starts talking, or it collides on any idle-gap design.
