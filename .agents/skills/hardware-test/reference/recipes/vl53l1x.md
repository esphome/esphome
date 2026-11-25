# vl53l1x

## Modules with their own microcontroller, register readback (#21916)

- The HiLetgo TOF400F carries its own microcontroller, which is a permanent I2C master on the exposed SDA and SCL
  pins (about 60 kHz, polling register 0x0031 of the VL53L1X at 0x29 forever). The ESP cannot share that bus:
  `i2c: scan` reports "SCL is held LOW" and random hits, and reads at 0x29 fail. Read such a module over its UART,
  or use a plain breakout to test the component.
- `scripts/esp8266_edges/i2csniff.yaml` (ESP8266, INPUT_PULLUP on GPIO4/5, waits for 300 us idle, records up to
  1500 level changes of SDA | SCL << 1) and `i2cdec.py CAPTURE` (start, stop, bytes and acks) show whether a
  module's I2C pins already carry another master. Run it before blaming wiring.
- A plain breakout (CQRobot, no microcontroller) answers only at 0x29 and reads `EA CC` at 0x010F.
- Resetting the ESP8266 in the middle of a sensor transfer can leave the VL53L1X holding SDA low across later
  resets: nine fixed recovery pulses can end in the ACK of a sensor that was receiving a write (#22852 pulses only
  while SDA is low). `i2cunstick.yaml` clocks SCL while SDA is low and then idles; flash it before each test build and capture without `--reset`, so the upload reset lands on an idle bus.
- `vl53l1x-regs-common.yaml`: package that logs 0x001E (offset, mm x 4) and 0x0060 to 0x0081 (timing, VCSEL
  periods, 0x0066 signal rate in 9.7 fixed point, 0x007F ROI centre, 0x0080 ROI size) every 20 s. Add a `vl53l1x`
  sensor with `on_raw_value` logging, since sensor state lines are VERBOSE only.
- A fixed target reads with an IQR of 1 to 3 mm, so a PR versus base difference above that is real.
