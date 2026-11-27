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

## Speed sensor timeline (#23108)

`scripts/vl53l1x/timeline.yaml` runs a scripted timeline on an ESP8266 (steady, alternating valid and NaN ranges by
rewriting the range status threshold registers, an XSHUT reset with setup() again, three update intervals) and
logs every range, speed and phase with the loop timestamp. `timeline_ana.py CAPTURE` checks that every speed equals
the range change over the time since the previous valid range, that no speed follows a NaN, and that the first
valid range after a NaN publishes none.

- At about 2.7 m the return is about 0.5 Mcps: with the default `signal_rate_limit` of 1.0 every frame is raw status
  4 (signal fail); `signal_rate_limit: 0.25` gives valid frames with an sd of about 11 mm.
- Per-frame logging of long lines at 115200 limits a fast loop to about 26 lines/s; throttle the log before
  measuring a frame rate.
- ESP8266 A/B size builds: give both configs names of the same length and last character. RAM and flash move by a
  few bytes with the name (a name ending in `0` shares a `"0"` literal).
