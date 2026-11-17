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

## TRF7962A fake chip (#22188)

`scripts/trf7962a_sim/`: `trfsim.hpp` replaces the SPI component's bus (`SPIComponent` is final, so `spi_bus_` is
reached through the explicit-instantiation member pointer trick) with a `FakeBus` from `on_boot` priority 800, between
the bus (1000) and the reader's setup (600). The bus hands both of the reader's clients (MODE0 write, MODE1 read)
delegates on one register, FIFO and IRQ model; the IRQ pad is switched to input/output after setup and driven by the
model. `trfpr.yaml` runs the tag scenarios (none, present, slow, flaky, swap, collision, CRC, error flag, bad MSB,
TX only, stuck IRQ) and prints per-phase counters (CS, field timing, TX payload, reads without an address). The
same FakeBus approach works for any SPI chip whose reply depends on the address byte inside one CS window, which the
ESP32 SPI slave cannot fake.
The header targets the SPI API of the #22188 branch (`SPIBus` and `SPIComponent::spi_bus_`); dev has since dropped
`SPIBus`, so on a newer tree the bus hook has to move to the new delegate creation before it builds.

## SX127x fake chip (#22733)

`scripts/sx127x_sim/`: `sxsim.hpp` is a fake SX1276, `sxrun.hpp` the scenario runner, `sxsim.yaml` the ESP32-PICO-D4
config (no Wi-Fi; `sxpr.yaml` and `sxbase.yaml` are two-line wrappers setting `substitutions: flags: -DSXSIM_PR` or
`-DSXSIM_BASE` and `packages: sim: !include sxsim.yaml`, since `-s flags -D...` is parsed as an option).
- Hook, no private access needed on the current SPI API (`SPIBus` is gone; `register_device()` builds the delegate in
  the client's `spi_setup()`): the bus is `interface: software`, and at on_boot 800 `set_clk/set_mosi/set_miso` on the
  bus and `set_cs_pin` on the radio swap in fake `GPIOPin`s, so the real `SPIDelegateBitBash` clocks every bit into
  the model (the MISO read drives the bit engine, as the PR's own gtest helper does). Works on any driver revision.
  The fake pins live in `spibits.hpp` (`PinSet` bound to a `SpiChip` with per byte reply and receive hooks, a lock
  hook and an `after_end()` that runs outside the lock), shared by `sxsim.hpp`, `sxair.hpp` and `sxrec.hpp` and
  buildable on the host; ESPHome copies only the files in `includes:`, so every config lists it.
  `InternalGPIOPin` is a final platform class alias, so rst and DIO0 cannot be faked on old drivers; rst is a real
  pad and a RegVersion read resets the model instead. DIO0 is GPIO4 with pulldown, switched to INPUT_OUTPUT at on_boot
  399 and driven by the model, so the rising edge interrupt really fires.
- Send completion runs from a one-shot `esp_timer` (`M.tx_ms`), because a blocking `transmit_packet()` spins on the
  DIO0 pad with no SPI traffic and no loop; model state is shared under a `portMUX`.
- Faults: `M.fail_mode_writes = N` (RegOpMode writes ignored, mode reads back old), `M.tx_never`,
  `sxsim::arm_rx_on_standby({...})` (a packet completes just as a send writes standby), `M.version`, CRC error inject.
  The chip ignores mode requests other than standby/sleep while a send is still going out (counter `ign_tx`).
- The FSK FIFO is one 64 byte queue for TX and RX, cleared on sleep and on entering RX, so a send that writes over a
  waiting FSK packet transmits the received bytes (dev did that in #22733's test). LoRa TX leaves the chip in standby
  with TX_DONE set; FSK stays in TX with PacketSent.
- `sxrun.hpp` boot cases use `RTC_NOINIT` plus `esp_reset_reason() == ESP_RST_SW` (a flash or EN reset starts at case
  0): 0 LoRa and FSK sequence (traces of one blocking send per modulation as `TRACE` lines, receive, rx before send, raw
  set_mode_tx, async OK/BUSY/INVALID/timeout/cancel, actions during a send, runtime mode failure with a 6 s watch),
  1 blocking send that never completes, 2 wrong version, 3 mode failure at setup, 4 `ALLDONE`. One 75 s capture with
  `--reset` covers all of it. `STATE` lines print the model counters.
- On #22733, dev lost a packet waiting at send start (LoRa) or sent it as its own (FSK), delivered a stale packet after
  raw TX mode (LoRa) or two empty ones (FSK), and was marked failed after a blocking send timed out because RX is
  refused while the chip is still in TX. The PR passed every case.

## Two radios and a simulated air (#21110)

`scripts/sx127x_sim/sxair.hpp` (models and air), `sxlink.hpp` (runner), `gen_pair.py OUTDIR` (writes `lpair` two SX127x
LoRa, `fpair` two SX127x FSK, `xpair` SX127x plus SX126x LoRa, each with a `<name>_glue.h`). One ESP32-PICO-D4
firmware, two software SPI buses; at on_boot 1100 (before the buses' setup at 1000) the bus pins, CS, reset and SX126x
busy are swapped for fake pins (needs the `GPIOPin *` reset setter from #22733), so the only pads used are the interrupt
lines GPIO4 (A) and GPIO22 (B), switched to input/output at on_boot 300. Shared real pin numbers need
`allow_other_uses`.
- SX126x fake: command parser on CS rise (SetPacketType, SetModulationParams, SetPacketParams, SetDioIrqParams,
  ClrIrqStatus, WriteBuffer, SetTx/SetRx/SetStandby/SetSleep), replies per byte position for ReadRegister (version
  string at 0x0320), ReadBuffer, GetIrqStatus, GetRxBufferStatus, GetPacketStatus; BUSY is a fake pin that is always
  low; IRQ bits latch only when in the IRQ mask, DIO1 = irq & dio1 mask.
- Air: airtime from the sender's own registers and commands (Semtech LoRa formula; FSK bytes at the bitrate), delivery
  to every receiver that stayed in RX, RSSI and SNR per receiver, collisions give a CRC error, knobs `AIR.drop_next`,
  `AIR.corrupt_next`, `jam(radio, ms or UINT32_MAX)`, `tx_never`. A receiver shows a signal while a packet is on air
  (SX127x LoRa ModemStat, FSK IrqFlags1 sync match held until read, SX126x preamble/sync/header IRQs).
- Gotcha: both radios share one main loop, so while one radio's blocking send runs the other cannot read its FIFO
  and the next packet overwrites the unread one (first runs lost 12 of 53 and no HDLC link came up). The model
  holds a packet that lands while the previous one is unread and hands it over from the 1 ms air tick once the
  driver has read the previous one and 3 ms passed with no SPI on that radio (draining at the flag clear corrupts
  the read in progress). A held packet must clear the receiver's signal, or listen before talk sees a phantom busy
  radio. Counters `held` say how often it happened; say in results that this is a simulator artifact.
- Raw links share the radios with the layered links, so a raw send can be refused by listen before talk while the
  layered links talk; retry. Raw test packets start with 0xEE and carry a length-derived pattern.
- `tx_over_signal` counts sends started while the sender showed a signal: 0 outside the hold cap phase means listen
  before talk never sent over a packet.

## SX127x SPI byte stream A/B (#22776)

- Host: copy `scripts/sx127x_sim/sx127x_spi_record_test.cpp.example` as a `.cpp` into `tests/components/sx127x/` of
  each tree and run `SX127X_RECORD=<file> script/cpp_unit_test.py sx127x` (the env var reaches the test binary). It
  sweeps FSK and LoRa frequency, bitrate, deviation and preamble edges through setup, configure() and 1 byte and
  largest sends against a register file with burst auto-increment (FIFO address fixed), and writes CASE/T/REGS/FIFO
  records. Case labels must be unique (an index is prefixed) or the compare silently drops cases. `compare_spi.py BASE
  PR` checks register files and FIFO exactly and compares the transactions flattened into per-register operations, so
  only grouping may differ.
- Device: `sxrec.hpp` with `recsw.yaml` (software bus, fake pins at on_boot 1100) or `rechw.yaml` (ESP-IDF hardware bus
  `interface: spi2`), two short wrappers setting the substitutions of `rec.yaml`: a tee delegate robbed into `SPIClient::delegate_` at on_boot 800 sends every byte through the
  real SPIDelegateHw and feeds the model; the radio's own spi_setup() then logs "Device already registered" once and
  keeps the tee. SPI3 as a slave sniffer (CLK 18, MOSI 23, CS 5 routed in, nominal CS GPIO14 high) captures each chip
  select window up to 64 bytes and compares it with what the driver sent ("WIRE ok/differ" line). Records print as
  "REC ..." lines; compare_spi.py reads the two device logs directly (it cuts each line at "REC " and joins the
  "REC +" continuation lines). The host harness keeps its own register file and pins, so the copy step stays one
  file.
