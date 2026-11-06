# hlk_fm22x without a module: fake FM225 on cross-wired UARTs (#18817)

`scripts/hlk_fm22x`: classic ESP32, no wiring. UART1 (tx 25, rx 26) is the component's, UART2 (tx 26, rx 25) the fake
module, 115200 baud. `fm22sim.hpp` is a FreeRTOS task on core 0 that parses `EF AA type len len data xor` frames,
logs `MOD RX cmd 0x.. len` and answers:

- status, version (NUL padded), serial (29 characters plus `FF A5 08` junk, like a real module), the 203-byte face
  id list, delete (reply after `delete_ms`), delete all, user info, face reset, and reset (answers an interrupted
  scan with 2 when `reset_aborts`);
- VERIFY: three little endian face state notes, then the reply: match admin, match plain, reject 1, timeout result
  13, or silent;
- ENROLL (interactive: face id -1 until the direction mask is 0x1F), ENROLL_SINGLE and ENROLL_ITG (40-byte payload,
  allow_duplicate at offset 35, refused with 0x0A when `known_face`).

Knobs are atomics set from YAML lambdas: `alive`, `scan_mode`, `scan_ms`, `delete_ms`, and `inject` (1 bad
checksum, 2 garbage, 3 cut-off frame, 4 eye note, 5 unknown note, 6 ready note, 7 restart with 1.5 s of silence
and then a ready note).

- Start the task from `on_boot` priority 800: at 1000 it runs before the UART is set up and marks it failed.
- `fm22-cases.yaml` holds 12 cases that both dev and #18817 understand; `fm22pr-parts.yaml` adds 28 cases for the
  PR's API (cancel variants, single and ITG enroll, a bad name, a queue of 7, restarts in the middle of a command, a
  late delete reply, the module gone and back, notes, reset, delete all). The dev build uses `fm22base-parts.yaml`
  (flat binary sensor, no admin argument, enroll needs `direction`). Triggers and `on_value` log `EV` and `ENT`
  lines; count the outcomes per request with grep.
- Run: `build.sh NAME TREE`, then `flashcap.sh DIR NAME TREE SECONDS` (`PORT`, `SC` in the environment) with 190 s
  for `fm22pr` and 100 s for `fm22base`. It fails without the `DONE frames= bad=` line.
- Found on dev: a module that boots late is never read out, the matched name is published with 32 bytes of padding,
  a second action is dropped ("already active"), and a scan the module never answers blocks for 30 s with no
  trigger.
