# SPI bus capture on the same board, no chips or wiring (#22376)

`scripts/spi_self/`: `gen.py` writes `spi<board>.yaml` (e32 classic ESP32, c3, e66 ESP8266, rp Pico 2 W) with a
hardware bus, a software bus, several `spi_device` instances (all four modes, MSB and LSB first, different rates and
CS pins) and a `max7219`. `spitest.hpp` drives every device from a 20 ms interval (an 8 byte transfer, write16, a 12
bit write, read_array, write_array, back to back transfers, teardown plus setup, a second setup, a max7219 update),
three passes, then reboots. `run.sh BOARD LABEL SECONDS` (env `TREE`, `PORT`, `SC`, `PY`, and `PORTS` for the Pico)
builds, flashes, captures and checks; `analyze.py spi<board>.yaml BASE.txt PR.txt` checks each record against the
expected wire data and diffs the two builds by the majority payload per record, so interrupt jitter does not count.

- Classic ESP32: the ESPHome bus is on SPI2 through the matrix and SPI3 runs as a slave without DMA (64 bytes at
  most), set up again in the device's mode for each operation. Its nominal CS must be a spare pad held high
  (GPIO14); the device CS is routed into `spics_in` only after `spi_slave_queue_trans()`, because the slave driver
  reconnects the nominal pad when a transaction loads. Never give the slave the device's CS pad:
  `spicommon_cs_initialize` takes over its output and `spi_slave_free` resets it. `gpio_set_direction(..., OUTPUT)`
  turns a pad back into a plain GPIO output, so only call `gpio_input_enable` on pads the master drives.
- With that slave attached, full duplex reads on the master lose their last bytes after the first read of a boot,
  even through a MOSI to MISO loopback; a config without the slave reads fine. Do not judge reads on that layout.
- The slave sees bit-banged mode 1 data one bit late; the interrupt captures decode it correctly.
- C3, ESP8266 and Pico 2 W: GPIO edge interrupts on SCK and CS take snapshots of the input register. C3 SPI2 cannot
  clock below about 78 kHz; at 80 to 100 kHz the capture works with
  `gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM)`, but one build lost an SCK pair at the start
  of a frame in every run and a rebuild did not: compare two builds before calling a missing edge real. Let the log
  output drain (20 ms) before a capture that follows `spi_setup()`.
- ESP8266: the HSPI pads (12, 13, 14) do not show in `GPI` while in SPI function; the script logs `SPI1CLK`,
  `SPI1C`, `SPI1U` and `SPI1P` after each hardware bus operation instead.
- Pico 2 W: `IO_BANK0` INOVER on the MISO pad forces the SPI receive input low or high (reads 00 and ff).
- Software bus loopback: MOSI and MISO on one pad with `allow_other_uses`, then set the pad back to output.
- Seen on dev before #22376: the software bus ignores `data_rate` (`cycle_clock_()` never waits), and SCK moves to
  the new idle level only after CS is low when devices with different clock polarity share the software bus.
- LibreTiny (BK72xx, RTL87xx, LN882x) has no `spi:` (config validation allows esp32, esp8266, nrf52 and rp2 only),
  so SPI changes cannot be run there.
