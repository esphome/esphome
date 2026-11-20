# SCD30 clock stretching and timeouts (#22512)

`scripts/scd30/`: `s30common.yaml` package (ESP8266 NodeMCU, SDA GPIO4 (D2), SCL GPIO5 (D1), VERY_VERBOSE I2C logs),
`s30def.yaml` default bus, `s30meas.yaml` with `scl_stretch.hpp` (times every SCL low phase), `s30unst.yaml` (frees a
stuck SDA at boot), `run.sh NAME YAML PORT SECONDS` (build, upload, capture, `ana.sh` counts). Make timeout variants by adding
`timeout: Nms` under `i2c:`. The SCD30 needs no external pull ups on that board and answers at 0x61.

- Measured on a real SCD30 (firmware 3.66) at 50 kHz: steady reads (data ready 0x0202, read 0x0300) stretch SCL at most
  143 µs; two 12.9 ms stretches happen in setup, on "set measurement interval" (0x4600). So `timeout: 20ms` works and
  5 ms or 1 ms fail setup with a NACK there.
- ESPHome on ESP8266 calls `Wire.begin()` and applies the bus `timeout:` before every transfer, so a component's own
  `Wire.setClockStretchLimit()` is overwritten at the next transfer.
- One reading every 2.3 s with `update_interval: 2s` (500 ms data ready poll); 10 min gives about 260.
- A setup that fails with a 1 ms timeout leaves the SCD30 holding SDA low across resets, and the 9 pulse recovery
  before #22852 could not free it (see [i2c_bus_recovery.md](i2c_bus_recovery.md)). `s30unst.yaml` pulses SCL until
  SDA rises, then sends a STOP; check `unst: before: SDA=0` on the next boot. 5 ms failures did not wedge it.
