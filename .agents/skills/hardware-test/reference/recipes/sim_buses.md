# Simulated chips on the bus, fake Home Assistant, static buffers

`scripts/sim_buses/` and `scripts/static_buffer/`. Each header is added with `esphome: includes:` and wired in an
`on_boot` lambda, so no chip is needed. The wrappers take the real bus or SPI delegate and pass through every
address they do not simulate.

- `lcdsim.hpp`: `LcdSimBus`, an `i2c::I2CBus` wrapper that plays PCF8574 backpacks (P0 RS, P2 EN, P4 to P7 D4 to D7)
  driving HD44780 controllers. It decodes the EN-strobed nibbles and rebuilds the rows written after each 0x80 and
  0xC0 address command. `add()` up to three addresses, hand it to each `lcd_pcf8574` with `set_i2c_bus()` at
  `on_boot` priority 800, and call `report()` from an interval. `lcd_an.py SERIAL` checks the rows against the
  patterns its docstring lists (0x27 20x4, 0x26 16x2, 0x25 64x4), and fails on a display whose frame count does not
  grow between its first and last report, on a segment longer than its rows or on
  data past the 128 byte segment buffer, which the report counts as `overflow`, on a report from an address it does
  not expect, or on any read (`reads`; the driver never reads, so the simulator NACKs them). A 64x4 display hangs the
  code before #22331 in
  setup.
- HT16K33: use `templates/i2c_fake/htfake.hpp` with its test runner (see [fake_i2c.md](fake_i2c.md)).
- `mxsim.hpp`: `MxSniff`, an `spi::SPIDelegate` in front of the real one that keeps what each MAX7219 in a chain
  latches. Swap it in at `on_boot` priority 399 through `DelegatePeek::of(id(x))`:
  `auto &d = DelegatePeek::of(id(x)); d = new MxSniff(d, chips);`. `mx_an.py SERIAL CHAIN...`
  summarizes the reports, CHAIN being each name passed to `report()`, and fails on a named chain with no report line,
  on malformed transactions (byte count not two per chip, or a partial byte from `write()` or
  `write_cmd_addr_data()`, which are recorded and forwarded too), on a named chain with fewer than two report lines,
  or a chain whose transaction count stays 0 or stops growing, and prints PASS otherwise.
- `spysim.hpp`: the same delegate trick (both headers carry `DelegatePeek`, so one config can include both), logging length and FNV-1a hash of every transaction over 1000 bytes (whole
  frames, used for `pixoo`). `write()` and `write_cmd_addr_data()` are hashed and forwarded like `write_array()`;
  a partial byte or a bus width other than 1 is hashed as whole bytes, so the SPY line carries `odd`, the count of
  such transactions, and a dev vs PR hash match only holds when `odd` is 0 on both.
- `pisim.hpp`: drives two `packet_interface` stacks on cross-wired UARTs through `top()`, with payloads whose bytes
  are a function of their index so every byte is checked, and the index and length each side sent (last 256) so a
  truncated or split packet, or one whose slot a later index reused, counts as `rx_bad`. Send indices 0, 1, 2 and so
  on from each side: a skipped index counts as `rx_lost` and an old or repeated one as `rx_bad`. `report(tag, true)` once both links are idle ends each
  line in PASS only when that side received every packet the peer sent intact and the peer had no send refused;
  `missing` is the rest, packets still in flight or lost at the tail. `packet_transport` drops packets carrying its own device
  name, so a one-board test must use the layers directly.
- `hcsim.hpp`: a software SN74HC165 chain on ESP-IDF (data GPIO6, clock GPIO7, load GPIO10). A falling load edge
  latches the pattern and each rising clock edge drives the next bit onto the data pad, which is in
  `GPIO_MODE_INPUT_OUTPUT` so the component reads it from the same pin. Call `hcsim::report(label, loads0, clocks0)`
  after each window with `hcsim::loads` and `hcsim::clocks` from its start; it logs `INVALID` instead of a result
  when the GPIO or interrupt setup failed or the window saw no load or no clock edge, so a dead simulator cannot pass
  as a wrong but steady pattern.
- `ledprobe.hpp`: samples a status LED (or any output) pin every 2 ms from an `esp_timer`, off the loop task, and
  prints blink count, on time, period and the longest gap per window. It measures `status_led` while the loop is
  blocked (#16407). Pass `ledprobe::n` from the window start as the mark; a window with more than 400 edges logs
  `INVALID` and no result line instead of passing for a stalled LED. So does every window when the pin or timer
  setup failed or the timer has not sampled for 100 ms, so a dead probe cannot read as a quiet LED.
- `fakeha.py HOST OUT_PREFIX`: an aioesphomeapi client standing in for Home Assistant for `homeassistant` select
  mirrors. It answers state subscriptions from a table, pushes option and state changes (full, overfull, quoted,
  unparseable), checks each step through a "Dump" button, captures the action calls, churns 600 option updates
  while watching the heap, and checks the entity options after a reconnect. A step fails when either DUMP line is
  missing, and a rejected update also needs the device's rejection log line, so an update that never arrived cannot
  pass. Exits 1 on a failed step and prints PASS otherwise.
- `wizard_drive.py HOST`: finishes the device wizard through aioesphomeapi 46.28 (`device_wizard`,
  `wizard_input_set`), answers the subscriptions the inputs create and checks the values reached the entities.

## Static buffers (`cg.static_buffer`)

`scripts/static_buffer/` holds two local external components and `static-buffer-dut.yaml`, which loads them:
- `sbtest` allocates three buffers with `cg.static_buffer` (37 `uint8_t`, 5 `uint32_t`, and `const char *` sized by
  a C++ expression) and checks at setup that they are zeroed and aligned, then fills each completely; at
  `dump_config()` it checks no other instance's fill reached them. Two instances prove the buffers are distinct.
  Both `SBTEST` lines end in `PASS` or `FAIL`; the `dump_config()` one passes only when the setup checks passed too.
- `sbpsram` asks for `psram=True` and uses the pointer codegen hands it: the static buffer without PSRAM, or a
  zeroed `psram_buffer<T>()` with a `psram:` block, which logs `external=1` (nullptr if the allocation failed).
  It also gets a plain 8 word `uint32_t` static buffer, which must be non-null, aligned and zeroed at start and read
  back its fill (`plain=1`). Its `dump_config()` line ends in `PASS` only when both readbacks match, both buffers
  were zeroed at start and, with PSRAM, the PSRAM one is in external RAM.

## Gotchas

- Some "classic" devkits with a CP2102N carry an ESP32-PICO-D4. GPIO16 and GPIO17 are its flash pins, and an
  ESP-IDF build that drives them hits the interrupt watchdog. Use `board: pico32` and GPIO9/GPIO10.
- An `addressable_light` display writes nothing until its light is turned on.
- Before calling a rebased PR's change trivial, diff the PR's own diff (head against its parent) old against new: a
  move that looks like a merge only can carry real commits from below.
