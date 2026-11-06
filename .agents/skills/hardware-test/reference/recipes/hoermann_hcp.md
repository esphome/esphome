# hoermann_hcp without a drive: fake HCP2 bus master (#21094)

`scripts/hoermann_hcp`: classic ESP32, no wiring. UART1 (tx 25, rx 26) carries `modbus` (role server) plus
`hoermann_hcp` at address 2; UART2 (tx 26, rx 25) is the fake drive. Both run 57600 8E1 (`allow_other_uses` on the
pins). `hcpsim.hpp` runs a fixed scenario in a FreeRTOS task on core 0 and logs every TX and RX frame,
`CHECK PASS` / `CHECK FAIL` lines and `DONE pass=N fail=M answer=...`:

- Bus scan: FC 0x17 to address 2, read 5 at 0x9CB9, write 3 at 0x9C41 (`cnt<<8|02`, 0, 0x0102). Dev answers
  `0000 0205 0430 10FF A845`, #21094 `0000 022E 0520 0800 0000`; the counter and command byte are echoed in
  registers 0 and 1.
- Status poll: read 8 / write 2 (`cnt<<8|03`, 0); a queued command shows in registers 2 and 3 (open 0110, light on
  0880).
- Broadcast: FC 0x10 to address 0, 9 registers at 0x9D31 (register 1 position 0 to 200, register 2 state in the high
  byte, register 6 lamp bit 0x10).
- `open_door()` and `set_light()` are not thread safe: the task sets `want`, a 20 ms interval runs them on the main
  loop and logs `STATE door= pos= valid= light=` changes.
- Also covers 3 s of silence (connection lost after 2 s), a rescan, a scan of another address (no reply), an
  unknown read length (zero padded), a wrong read address (exception 2) and 20 back-to-back scans. One run takes
  40 s of capture.
- `build.sh hcppr TREE`, then `flashcap.sh DIR hcppr TREE 45` (`PORT`, `SC` in the environment) uploads the build,
  captures with `--reset --ms` and fails unless the `DONE` line has `fail=0`. Copy `hcppr.yaml` with another name
  for the base build.
- Entity state changes are not logged at DEBUG on current dev; log from lambdas instead.
- Limit: only the frames are checked; whether a real drive accepts the answer is out of reach.
- Result on #21094: both builds passed every check, only the scan answer differs.
