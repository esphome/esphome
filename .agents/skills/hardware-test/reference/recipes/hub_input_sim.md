# matrix_keypad, wiegand, rdm6300 and key_collector without hardware (#21787, #21788, #21790)

All on a D1 mini with no wiring: `scripts/hub_input_sim/hub.yaml` and `sim.hpp`.

- Keypad: one row (GPIO5) and columns GPIO4, 14 and 12 with pull-ups. `sim::press(pin)` switches the column to
  `OUTPUT_OPEN_DRAIN` driven low, so the single row reads that key pressed on every scan; `sim::release` goes back
  to `INPUT_PULLUP`.
- Wiegand: D0 on GPIO13, D1 on GPIO0. Call `sim::wiegand_arm()` once, then `sim::wiegand(value, bits)` sends the
  pulses (80 us low, 1.5 ms apart, most significant bit first). `pinMode(OUTPUT_OPEN_DRAIN)` clears the pull-up and,
  with the output latch low, makes a falling edge that adds a bit: write `GPOS` high before the mode switch and set
  `GPF |= 1 << GPFPU` after. The interrupt type survives `pinMode`.
- rdm6300 reads UART0 with its loopback bit set (`USC0(0) |= 1 << UCLBE`), so the logger runs with `baud_rate: 0`
  and the log goes over the API. Frames are 0x02, 12 ASCII hex characters (version, 4 id bytes, xor) and 0x03. The
  uid binary sensor pulses on and off per read.
- key_collector takes both the keypad and wiegand as `source_id`.
- `run.sh LABEL` flashes over serial, reads the API log until `DONE` and writes `out-hubsim<LABEL>.ev`; diff the
  `[ev` lines of dev and the PR.
- Wiegand frames with valid parity: 26 bit 0x3579bdf (11259375), 34 bit 0x3bd5b7dde (3735928559), 37 bit
  0x1b4b4b4b4a (24253932965). 0x3579bdd fails parity, so only `on_raw` fires for it.
- rc522 and sx1509 need their chips; compile them only.
- Results: https://github.com/esphome/esphome/pull/21790#issuecomment-6491675405
