# Crash report over the API (#21596, #22288)

#21596 lets an API client fetch the crash recorded on the previous boot; #22288 records `abort()` as a crash on
LibreTiny. `scripts/crash_report/crash-common.yaml` (a package: Wi-Fi from `secrets.yaml`, plaintext API, OTA,
DEBUG logger) adds three buttons: `crash abort`, `crash null store` and `crash div zero`. The per-run file supplies
the board and the name. Flash it, then run the drivers (they import `scripts/apiconn.py`, so run them in place or
put `scripts/` on `PYTHONPATH`):

- `drive.py HOST BUTTON_OBJECT_ID` (for example `crash_abort`) reads the capability, presses the button, reconnects
  after the reboot and fetches the report twice (reason, PC, cause, fault address, build time, per-core backtrace),
  subscribes to logs with `dump_config` to see what is replayed, and checks the capability from a second client.
  It reads an earlier report first, so a stale one cannot pass. It exits 1 when the device still answers after the
  press, does not offer a report after the crash, or the report has no reason or a PC of 0.
- `peek.py HOST` prints only the capability and how many replayed `CRASH DETECTED` lines a new log client gets. To
  only press a button, use `scripts/api_button.py HOST - press "crash abort"`.
- Decode the PC and the backtrace with the build's ELF (`addr2line -e firmware.elf`). On RTL8720C a noreturn
  `bl abort` can be the last instruction of a function, so the return address points at padding: decode
  `(addr & ~1) - 2` to get the call line.
- `__builtin_trap()` faults on every board. A store to `nullptr` does not fault on every chip (BK7238 and RTL8720C
  ignore it), so the `crash null store` button proves nothing there.
- With #22288, `abort()` reboots BK7238, RTL8720C and LN882H boards in 1 to 2 s and logs `Reason: Abort` with the PC
  at the call site. Before it, the BK7238 hangs until reset, the RTL8720C restarts with no record and the LN882H
  waits about 18 s for its watchdog.
- Quick crash reboots count toward BK72xx safe mode, so space out back-to-back crash tests on those boards.
- A device that keeps the previous crash replays it to every new log client until the next crash; that is one
  record, not two.
