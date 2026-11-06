# Simulated chips on the bus, fake Home Assistant, static buffers

`scripts/sim_buses/` and `scripts/static_buffer/`. Each header is added with `esphome: includes:` and wired in an
`on_boot` lambda, so no chip is needed. The wrappers take the real bus or SPI delegate and pass through every
address they do not simulate.

- `lcdsim.hpp`: `LcdSimBus`, an `i2c::I2CBus` wrapper that plays PCF8574 backpacks (P0 RS, P2 EN, P4 to P7 D4 to D7)
  driving HD44780 controllers. It decodes the EN-strobed nibbles and rebuilds the rows written after each 0x80 and
  0xC0 address command. `add()` up to three addresses, hand it to each `lcd_pcf8574` with `set_i2c_bus()` at
  `on_boot` priority 800, and call `report()` from an interval. `lcd_an.py SERIAL` checks the rows against the
  patterns its docstring lists (0x27 20x4, 0x26 16x2, 0x25 64x4). A 64x4 display hangs the code before #22331 in
  setup.
- `htsim.hpp`: `HtSimBus`, a fake HT16K33 at 0x70 that keeps the 16 RAM bytes (`ht16k33_char`). The full HT16K33
  fake with a test runner is `templates/i2c_fake/htfake.hpp` (see [i2c_slave.md](i2c_slave.md)).
- `mxsim.hpp`: `MxSniff`, an `spi::SPIDelegate` in front of the real one that keeps what each MAX7219 in a chain
  latches. Swap it in at `on_boot` priority 399 through `DelegatePeek::of(id(x))`:
  `auto &d = DelegatePeek::of(id(x)); d = new MxSniff(d, chips);`. `mx_an.py SERIAL` summarizes the reports and
  fails on malformed transactions (byte count not two per chip).
- `spysim.hpp`: the same delegate trick, logging length and FNV-1a hash of every transaction over 1000 bytes (whole
  frames, used for `pixoo`).
- `pisim.hpp`: drives two `packet_interface` stacks on cross-wired UARTs through `top()`, with payloads whose bytes
  are a function of their index so every byte is checked. `packet_transport` drops packets carrying its own device
  name, so a one-board test must use the layers directly.
- `hcsim.hpp`: a software SN74HC165 chain on ESP-IDF (data GPIO6, clock GPIO7, load GPIO10). A falling load edge
  latches the pattern and each rising clock edge drives the next bit onto the data pad, which is in
  `GPIO_MODE_INPUT_OUTPUT` so the component reads it from the same pin.
- `ledprobe.hpp`: samples a status LED (or any output) pin every 2 ms from an `esp_timer`, off the loop task, and
  prints blink count, on time, period and the longest gap per window. It measures `status_led` while the loop is
  blocked (#16407).
- `fakeha.py HOST OUT_PREFIX`: an aioesphomeapi client standing in for Home Assistant for `homeassistant` select
  mirrors. It answers state subscriptions from a table, pushes option and state changes (full, overfull, quoted,
  unparseable), checks each step through a "Dump" button, captures the action calls, churns 600 option updates
  while watching the heap, and checks the entity options after a reconnect. Exits 1 on a failed step.
- `wizard_drive.py HOST`: finishes the device wizard through aioesphomeapi 46.28 (`device_wizard`,
  `wizard_input_set`), answers the subscriptions the inputs create and checks the values reached the entities.

## Static buffers (`cg.static_buffer`)

`scripts/static_buffer/` holds two local external components and `static-buffer-dut.yaml`, which loads them:
- `sbtest` allocates three buffers with `cg.static_buffer` (37 `uint8_t`, 5 `uint32_t`, and `const char *` sized by
  a C++ expression) and checks at setup that they are zeroed and aligned, then fills each completely; at
  `dump_config()` it checks no other instance's fill reached them. Two instances prove the buffers are distinct.
- `sbpsram` asks for `psram=True` and uses the pointer codegen hands it: the static buffer without PSRAM, or a
  zeroed `psram_buffer<T>()` with a `psram:` block, which logs `external=1` (nullptr if the allocation failed).

## Gotchas

- Some "classic" devkits with a CP2102N carry an ESP32-PICO-D4. GPIO16 and GPIO17 are its flash pins, and an
  ESP-IDF build that drives them hits the interrupt watchdog. Use `board: pico32` and GPIO9/GPIO10.
- An `addressable_light` display writes nothing until its light is turned on.
- Before calling a rebased PR's change trivial, diff the PR's own diff (head against its parent) old against new: a
  move that looks like a merge only can carry real commits from below.
