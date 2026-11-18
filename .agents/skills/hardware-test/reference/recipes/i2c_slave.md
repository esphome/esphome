# Fake I2C slave (#21586)

`scripts/i2c_slave`: a D1 mini running an Arduino `Wire` slave at 0x40 (`i2cslave.yaml` with `i2cslave.hpp`): a fake
HDC1080 plus a 256 byte register file, and reads of 0x7E and 0x7F stretch the clock for 0.5 and 5 ms. It needs
`libraries: [Wire]` and 160 MHz. With jumper wires only and the internal pull ups it works to about 50 kHz; it cannot
keep up with a true 100 kHz hardware master.

The master (`i2cbk.yaml`, a BK7238 on P26/P24, needs the software I2C bus from that PR) writes a counter, reads it
back with a repeated start, probes an absent address and reads both stretch registers every 2 s, logging `RW OK`
(the read back matches, the absent address NACKs and both stretched reads succeed) or `RW BAD` with running totals.
Control: the same blocks on a classic ESP32 devkit (SDA GPIO21, SCL GPIO22) with hardware I2C at 25 and 50 kHz got
every repeated start read right, so failures on a bit banged master at a similar real clock point at the master.
