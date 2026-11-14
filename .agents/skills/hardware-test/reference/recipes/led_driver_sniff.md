# Bit-banged LED drivers without the chips (#21841 to #21847)

The GPIO LED drivers (bp5758d, bp1658cj, sm2235, sm2335, tlc5971, tlc5947, my9231, sm16716) never read the chip
back, so a classic ESP32 devkit can capture its own pads with no wiring. `scripts/led_driver_sniff`.

- `sniff.hpp`: SPI2 and SPI3 run as DMA slaves fed from the driver's clock and data pads through the GPIO matrix
  (`esp_rom_gpio_connect_in_signal`, pad input enabled with `gpio_input_enable`). Mode 1 samples on the rising edge
  of its clock input (measured, not what the mode table suggests): SPI2 takes the clock straight (rising-edge bits,
  `R` lines) and SPI3 inverted (falling-edge bits, `F` lines; my9231 clocks a bit on both edges). CS is GPIO2,
  driven by the test. PCNT counts clock and data edges and latch rises per window.
- ESP32 slave DMA drops a partial last word, so `close()` pads the clock with whole pulses to a 32-bit boundary plus
  32, and the bit count comes from PCNT. `selftest()` sends a known 57-bit pattern with the clock idling low and
  high; `selftest.py` checks it, and `run.sh` runs it on every capture.
- bp5758d, bp1658cj and sm10bit_base (sm2235, sm2335) release the data line for the ack with `pin_mode(INPUT)`, and
  `pin_mode(OUTPUT)` disables the pad input. The `sniff` local component (`ext/`) links with
  `-Wl,--wrap=gpio_set_direction` and the header re-enables input on the sniffed pins. The ack bit reads the
  floating line, the same on every build.
- `gen.py` writes `led.yaml`: all eight drivers on separate pins (bp5758d 25/26, bp1658cj 23/4, sm2235 18/19, sm2335
  21/22, tlc5971 27/32, tlc5947 33/13 latch 14, my9231 16/17, sm16716 5/12, selftest 15/0) and a script per driver:
  one window per step (open, set levels, 200 ms of main loop, close), a repeated step that must send nothing, two
  direct `loop()` calls timed with `esp_timer`, and an idle window.
- `run.sh LABEL` builds, flashes, captures until `DONE` and checks the selftest. `compare.py BASE PR DRIVER` diffs
  every window of one driver between two runs and decodes it (9-bit groups for the BP and SM frames, 12-bit words
  for tlc5947, 16-bit for tlc5971, the two-edge interleave for my9231, 50 zero bits plus 25-bit groups for
  sm16716). Loop timing varies about 3 % between builds even for drivers a PR does not touch.
- my9231 toggles DCKI with `!digital_read()`. ESP32 output pins have the pad input disabled, so without the
  sniffer's input enable the read is always 0 and the clock never toggles (on dev too).
- Out-of-range channels: run `esphome config` with channel = last and last + 1 on dev and the PR.
- Results: https://github.com/esphome/esphome/pull/21844#issuecomment-6491228132
