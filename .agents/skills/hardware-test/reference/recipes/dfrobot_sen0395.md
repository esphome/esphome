# dfrobot_sen0395

## DFRobot SEN0395 mmWave on an ESP32 DevKit V1 (#20906, #20979)

Wiring: V to VIN (5 V, the module wants 3.6 to 5 V), G to GND, sensor TX to GPIO16 (RX2), sensor RX to GPIO17
(TX2), IO2 to GPIO18 (presence output, a gpio binary sensor), I1 unconnected. `templates/dfrobot-sen0395-dut.yaml`
has the hub, both detection binary sensors, the four switches, settings buttons and junk-line buttons.
`scripts/dfrobot_sen0395/ctl.py HOST KEY press:NAME sw:NAME=on|off wait:S` drives it and prints binary sensor and
switch updates; `cases.sh DEV_IP KEY CAPTURE LABEL` runs the whole flow against a running serial capture. Flash by
USB, `serial_capture.py PORT 540 CAPTURE --reset`, then the cases; compare the `Sending command` and `Message:` lines
and the counts of `Done`, `Error`, prompts and frames between builds. The module echoes every command, so the echoed
lines are the bytes the component sent and can be compared between builds as a set.

Module facts: it answers `Done` or `Error` and then the `leapMMW:/>` prompt without a line end; a junk line gets
`<line> is not recognized as a CLI command` (36 chars longer than the line, so 220 and 240 byte junk gives 256 and
276 char replies); input lines of 256 bytes or more make the module assert (`..\User_code\main.c line:232`) and
restart, after which the sensor is stopped; a factory reset restarts the module about 7 s later; `sensorStop` on a
stopped sensor answers `sensor stopped already` then `Error`; a settings press takes 5 to 6 s (stop, change, save,
start). Free heap on the ESP32 is Wi-Fi noise (192 to 198 kB) and says nothing about the component.
Results: https://github.com/esphome/esphome/pull/20979#issuecomment-6279129146 and
https://github.com/esphome/esphome/pull/20906#issuecomment-6279129604
