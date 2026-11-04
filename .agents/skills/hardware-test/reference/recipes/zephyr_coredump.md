# zephyr_coredump

## zephyr_coredump crash and decode on nRF52 boards (#13122)

`templates/nrf52-coredump-crash-dut.yaml` (dongle; change `board:` to `adafruit_itsybitsy_nrf52840` for the
ItsyBitsy): `zephyr_coredump:`, `dfu: true`, a `restore_value` global keyed by the `run` substitution
(settings survive a reflash, so bump `run` for every new crash), and an `on_boot` sequence that sets the
flag, yields 3 s so the globals loop saves it (a blocking lambda never gets there and the board crash-loops),
syncs preferences, then writes to 0xDEADBEEF. Flash, keep every port closed for 80 s, then
`scripts/zephyr_coredump/flow.sh LABEL PORT ELF [OUTDIR]` does the three opens (3 s cut, 100 s full print,
20 s reopen) and calls `decode.sh`, which keeps the first complete `#CD:BEGIN#..END#` block, runs Zephyr's
`coredump_serial_log_parser.py` and `coredump_gdbserver.py` from the sdk-nrf ESPHome installed and prints
`bt` and `info threads` with its `arm-zephyr-eabi-gdb`. The ELF is `.pioenvs/<name>/zephyr/zephyr/zephyr.elf`.
Expect about 9 KB dumps, 140 to 150 `#CD:` lines, a 673 ms loop warning for the erase, and `Has stored
dump: 0` on the reopen. Check `compiled on` in every capture: another session flashing the same board
mid-test looks exactly like a failed test otherwise. Boards without a network cannot exercise the API and
MQTT delivery paths of the component; say so in the results.
