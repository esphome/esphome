# MCP2518FD CAN FD over SPI (esphome#21951, 2026-11-07)

Bench: ESP32-PICO-D4 (`board: pico32`, ESP-IDF) wired to an MCP2518FD module with an ATA6563 transceiver: SCK GPIO25,
SDI GPIO26, SDO GPIO27, nCS GPIO32, INT GPIO33, module 3V3/GND, CLK pin to GPIO34. No second CAN node, so everything
runs in `LOOPBACK`, or with the controller forced into internal loopback behind the driver.

- **Read the crystal.** The module's can is marked `H20.00` (20 MHz) even though listings say "8 Mbps"; the PR default
  is `40MHZ`. Loopback never leaves the chip, so a wrong `clock:` still passes at half the configured rates. Measure it:
  `pulse_counter` on the CLK pin with `internal_filter: 0us` reads SYSCLK/10 (2 MHz) at reset. Older heads wrote 0 to
  `OSC` byte 0, which set `CLKODIV` to /1 (CLKO = 20 MHz, still countable by PCNT).
- `scripts/mcp251xfd/canlb.hpp`: stream checker (one frame in flight, burst of 8 every 64th; standard/extended IDs, FD
  with and without BRS, every 4th classic, DLC 0 to 15, sequence in the payload; starts 5 s after boot so the
  `dump_config` stall cannot trip the 50 ms in-flight timeout), a queue checker for bursts, RTR and TX objects, raw SPI
  register access on the driver's own `SPIDevice` (`rd32`, `wr8`, `chip_reset` = `0x00 0x00`), forced modes
  (`force_mode` writes `CON+3`; `force_via_config` goes through Configuration so the chip accepts it), `watch_start`
  (polls `CON` every 2 ms until `OPMOD` and `REQOP` equal the target, logs the time), `txcheck` (reads each TX object
  back from chip RAM after a frame full of 0xAA, padding included), `burst` (sends N frames in one call so the loop
  cannot drain RX: FD depth 20, CLASSIC depth 32 + one overrun) and a PCNT unit on the CS pin, which counts SPI
  transactions while the pin stays an output (`cs_mark`/`cs_report`: idle cost per second).
- `common-can.yaml` is the package; each config adds its `canbus` block and a `phases` script: `can-loopback.yaml`
  (stream, RTR, TX objects, idle cost, chip reset, two refused `REQOP` writes), `can-classic.yaml` (CLASSIC with a
  forced internal-loopback window: force once to trigger the driver's recovery, force again right after it restores,
  then burst 48 and RTR inside the next 5 s check interval), `can-normal-busoff.yaml` (no node acknowledges, so the
  controller cycles error passive/bus-off; each forced fault and reset waits for TREC `TXBO`).
  `run.sh CONFIG LABEL [SECONDS]` builds, flashes, captures and prints the summary lines.
- Gotchas found: a refused `REQOP` (LISTENONLY or Restricted from LOOPBACK) leaves `OPMOD` unchanged and stalls TX;
  the chip refuses LOOPBACK to CLASSIC directly (go through Configuration); after a chip reset TREC reads `0x00200000`
  (`TXBO` set in Configuration mode), so a driver that skips recovery on bus-off never recovers a reset during an error
  state. Neither a reset nor a refused request sets an interrupt flag, so detection waits for the periodic check.

Results: https://github.com/esphome/esphome/pull/21951#issuecomment-6715452893 (first run),
#issuecomment-6716105991 (TX objects, idle cost), #issuecomment-6716741859 (reset during bus-off bug),
#issuecomment-6716957543 (fixed).
