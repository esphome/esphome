# vl53l4cx

## ESP8266 bring-up, start-up frames and I2C sizes (#23215, #23219)

Bench: a NodeMCU with a VL53L4CX breakout: SDA GPIO4, SCL GPIO5, the sensor's GPIO1 (open drain data ready) on
GPIO14 with a pull-up, XSHUT on GPIO12.

- XSHUT has no pull-up that holds it high at boot, so `i2c: scan: true` finds nothing (the scan runs before any
  component drives XSHUT). Give the component `enable_pin:` before trusting a scan. After XSHUT high the chip ACKs
  within about 0.5 ms; at a changed address (0x2C) it only answers at 0x29 again after an XSHUT reset.
- ST's init writes 135 bytes at register 0x0001 in one transfer and ESP8266 Wire buffers 128 bytes, so setup failed
  with `i2c: TX failed` until the write was split; the largest read is 83 bytes. To check a bus's limits, write
  exactly the buffer size (register address included) and one byte more, read the same sizes, and read the model ID
  after each step. arduino-pico reports a transfer to an absent address as ERROR_UNKNOWN while an empty-write scan
  reports a NACK.
- Start-up: frame 1 always has 0 targets, frame 2 has status 6, and in about 1 of 5 starts the farthest target is
  the near object for 1 or 2 frames. With the interrupt pin 30.4 readings/s (33 ms budget); polling gives about 25/s
  plus about 7 ms of loop time per reading. The first value comes about 220 ms after each start.
- To simulate a sensor that never boots, hold XSHUT low from a timer1 interrupt that writes `GPOC = 1 << 12` every
  100 us; a lambda `digitalWrite` loses to the component's own reset pulse.
- Log rate, NaN count, mean and sd every 5 s from `on_raw_value`; a stray object in view makes `farthest` publish
  NaN half the time, so check `nearest` before blaming the change.
- With the protective film still on, the sensor sees a strong target at 4 to 5 cm and ranges nothing else.
