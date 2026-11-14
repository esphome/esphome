# Addressable LED strips (#21447, #21445)

- To exercise a failed strip allocation, use a classic ESP32 without PSRAM with `num_leds: 65535` and
  `channel_colors: GRBW` (262 kB, bigger than any internal block). `num_leds` is uint16 in the drivers, larger
  values wrap. On dev such a strip boot-loops the board.
- Never force allocation failures by hogging RAM instead: Wi-Fi then aborts in `operator new` or the loop task
  trips the task watchdog.
- On the ESP32-S3 give each RMT strip `rmt_symbols: 48`, or the first channel takes all 192 symbols.
- A board with a few WS2812 on a pin already defined as a light can be extended with `id: !extend <light id>` to
  add effects; restore its own config afterwards.
- Loop cost of a strip (#19337): give each strip an `addressable_lambda` effect with `update_interval: 0ms` that
  changes every pixel, and add `runtime_stats: log_interval: 30s`; compare the main loop and light times per pass,
  leaving out the first period after boot. The strips need no LEDs attached. A classic ESP32 has 512 RMT symbols in
  all, so three strips need `rmt_symbols: 128` each.

## RMT channel and memory budget (#22172)

`scripts/rmt_budget/`: each line of `cases.txt` is a mix of strips, transmitters and receivers with their
`rmt_symbols` (`s:96`, `t:def`, `r:48`, ...) on an ESP32-S3 or ESP32-C6, and `gen_case.py CASE` turns it into a
config whose strips have an `output_id` and which logs `STATUS so0=<is_failed> tx1=...` every 3 s. `run.sh CASE...`
builds each case under one name per board (so builds stay incremental), flashes, captures 15 s and prints the STATUS
line and the RMT driver errors; it exits 1 when a case printed no STATUS. `verdict.sh CASE...` runs only
`--only-generate` and prints the config errors, the RMT memory warnings and the components registered, to compare
with what the board then does. No LEDs are needed: allocation failures show at boot.

- On the S3 USB console the ESP-IDF `no free tx channels` lines are printed before the port reopens; the C6 on
  UART0 shows them.
- S3 TX channels can use RX memory (384 symbols at most) but only start on blocks 0 to 3, so two 192 symbol strips
  do not fit.
- 17 of 17 predictions of the PR matched the driver. Results:
  https://github.com/esphome/esphome/pull/22172#issuecomment-6615083139

## Capture the wire, not the buffer (#21577)

`templates/led_wire/wire.hpp` reads what the strips send, with no wiring:
- RMT RX channels on the `esp32_rmt_led_strip` pads at 10 MHz, decoded with a high-time threshold per chipset
  (ws2812 0.4/0.8 µs: threshold 7 ticks; sk6812 0.3/0.6 µs: threshold 4);
- an SPI2 DMA slave fed from the `spi_led_strip` pads, with `spi: interface: spi3` for the strip.

`led-wire-dut.yaml` (classic ESP32) drives a GRB ws2812 strip, a GRBW sk6812 strip, an SPI strip, an RGB light and a
partition through steps S0 to S8 (on, `addressable_set`, lambda writes, partition, effect, power limit, off), logging
the decoded `RMT`, `SPI` and buffer `GET` lines and `DONE` per step, then runs `stress_step()` over 300 shows and
logs bad frames per strip. Diff the step lines of dev and the PR.

- A comparison of the buffers alone missed a PR bug in which the start frame was not sent.
- Give the captured strip `rmt_symbols` that hold a whole frame (192) and capture one RX channel at a time: with 64
  symbols the TX refill interrupt stretches bits on dev and the PR alike while the capture runs. The substitutions
  at the top of the config pick the strip.

## rp2040_pio_led_strip: a self-sampling PIO probe (Pico 2 W)

`scripts/pio_probe/pioscope.hpp` samples any GPIO from PIO2 at 25 MHz while another peripheral drives it (PIO reads
the pad without taking the pin) and decodes WS2812-style pulses into bit timings and bytes; no wires or LEDs.
`pa.yaml` probes five strips on PIO0 and PIO1 (WS2812 and two custom timings), `pc.yaml` one strip after claiming
DMA channels 0 to 11. `run.sh pa|pc LABEL` builds, flashes, captures 40 s and prints the `RESULT` lines; a correct
frame of two LEDs set to `Color(0xA5, 0x3C, 0x0F)` reads `3ca50f3ca50f`.

- Turn the light on at full brightness before probing, or color correction writes zeros.
- The probe FIFO overruns once in a while: count only full captures. `run.sh` fails unless every strip gave at least
  one correct frame.
- An RP2350 `panic()` escalates to a HardFault and the rp2 crash handler reboots within about a second, so a panic
  during component setup boot-loops before USB CDC enumerates and the board looks dead (no port, no BOOTSEL drive).
  Hold BOOTSEL while plugging it in and flash the UF2 drive. To see such a fault, trigger it after boot.
