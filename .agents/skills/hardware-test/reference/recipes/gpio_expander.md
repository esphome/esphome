# GPIO expanders

## Interrupt driven base class (#21469, #21468)

Any device with a PCF8574 input expander whose INT line goes to an ESP32 GPIO works; the inputs should change
during the test (end switches on a gate or cover are ideal). Before flashing, check with `nm` that
`gpio_expander::interrupt_pin_isr` lands in IRAM (0x4008xxxx on a classic ESP32). Then confirm `dump_config` lists
the interrupt pin and the input states match reality, and that the `pcf8574` entry is absent from the runtime stats
while idle (its loop is disabled). Drive the inputs (for a cover, `cover_command(position=1.0)` and then `0.0` over
the API, waiting for each end state): every input edge should log, `pcf8574` should show a small count in that
minute's stats and vanish in the next. For a device that cannot be reached physically, boot the same code on a
bench board first and keep a merge base image built as the fallback; an ESP32 bootloader without rollback support
leaves only `safe_mode` to recover a bad image.
