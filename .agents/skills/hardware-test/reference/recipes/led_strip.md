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

## Colour correction A/B on the probe (#22868)

`scripts/pio_probe/color_correct/`: `color-correct.yaml` puts six 2-LED `rp2040_pio_led_strip` strips on the Pico 2 W
(WS2812 RGB and SK6812 GRBW at gamma 2.8, the same with `gamma_correct` omitted, RGB at gamma 1.0, and an RGBW strip
whose `color_correct` each case sets with `AddressableLight::set_correction()`) plus two `partition` lights over one
RGB and one RGBW strip in both orders. `cccases.hpp` steps a case list (brightness, `color_brightness`, RGBW, channel
corrections, optional direct pixel writes) and logs `IN` and `PIN` (the colour and local brightness the light passes
to the strip) plus three probe `RESULT` frames per strip. `model.py` rebuilds the gamma table like
`light/__init__.py` and applies the base and PR rules; `analyze.py BASE_CAPTURE PR_CAPTURE` compares every full
capture with the model. `run.sh LABEL` (env as for `pio_probe/run.sh`) flashes and captures 160 s, enough for two
rounds of every case.

- RGB-only lights carry white at 100% in `LightColorValues` (the default `white_`), so `color_from_light_color_values`
  passes W=255 to an RGB strip; any per-pixel rule over four channels must ignore it.
- Call `out->update_state(state)` before probing so the frame uses the correction just set.
- `pioscope.hpp` splits bits at the midpoint of the shortest and longest high pulse, or at 500 ns when they differ by
  150 ns or less (an all-zero or all-ones frame, which a midpoint decodes as noise).
- About 1 in 600 captures has a single 1 bit read as 0; judge a case by its other captures.
- Eight PIO strips is the limit here: `rp2040_pio_led_strip` takes PIO 0 or 1 only and PIO2 holds the probe.
- A partition takes its traits from its first segment. Before #22868, a partition whose first segment is RGB lit a
  later RGBW segment's white at the colour's level (white stays 100% in RGB mode); with it that white stays dark.

## RMT power management lock and light sleep (#22631)

Config: the issue's config with `profiling: true` under `esp32_power_management` (the lock and mode times need it), plus Wi-Fi, API, OTA, `logger: baud_rate: 0` (the native USB console holds a light sleep
lock otherwise, see `reference/boards.md`) and an interval that writes `esp_pm_dump_locks()` into an `fmemopen`
buffer and logs it line by line, so the lock table arrives over the API. An API driver walks idle, red, six changes
2 s apart, rainbow, effect None and off (65 s each), then soaks to 16 min, logging everything with host timestamps;
compare lock and mode times between dumps per step. Single digit percentages print as `5 %` in the mode rows, which
adds a field when splitting on spaces.

## Counting a PIO strip's edges on its own pin (#16243)

A GPIO interrupt on the strip's own data pin (`gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_RISE, ...)`)
counts every bit the PIO program sends, with no wires or LEDs; a strip that sends nothing shows 0 edges per report.
Log the PIO block's state machine enable mask and program counters next to the count. `pio_probe/pio-common.yaml`
clears PIO1 instruction memory at boot because it survives a reflash (see `reference/boards.md`).
