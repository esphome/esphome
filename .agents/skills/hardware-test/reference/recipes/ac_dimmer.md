# ac_dimmer

## Zero cross and gate timing loop harness on the ESP8266 (#22912)

No mains needed: a NodeMCU drives its own zero cross and measures its own gate. Files in `scripts/ac_dimmer/`.

- Jumpers: GPIO12 (D6) to GPIO14 (D5) and GPIO13 (D7) to GPIO4 (D2). `esp8266_pwm` at 100 Hz, 5 % on GPIO12 is the
  zero cross (falling edge, a simulated 50 Hz mains) into the dimmer's `zero_cross_pin: GPIO14`; the dimmer's
  `gate_pin: GPIO13` is timestamped on GPIO4.
- `acdm.hpp`: a falling edge interrupt on GPIO12 stamps each crossing and a change interrupt on GPIO4 stamps the gate
  edges; it counts gates per half cycle (missed, doubled, gate still high at a crossing) and compares the start and end
  of each gate with the time computed from the component's formula (RMS compensation, min power 10 %, 10 ms half
  cycle). Noise: from a 20 ms interval it drives GPIO12 high for 5, 50, 300 or 1000 µs, ending 2, 4, 6.5 or 8 ms after
  a real crossing, rotating.
- Sequence (`acd.yaml`): 15 s boot, a clean sweep of 10/25/50/75/100 % (22 s per level, first 2 s not counted), the same
  sweep with noise, then a 50 % hold with a report every 20 s. `run.sh NAME TREE leading_pulse|leading|trailing SECS
  [1]` builds one image per method, captures from a reset, and with `1` sends back to back web_server requests from
  the `HOLD start` log line. `ana.py` prints every window, `ana.py --sum` sums them per phase.
- Arduino's ESP8266 GPIO interrupt dispatch serves lower pin numbers first, so a gate edge on GPIO4 that is pending
  together with a crossing on GPIO12 is counted first. A gate that starts a few µs before the crossing edge it belongs
  to is counted against the previous half cycle; check the gate width before calling it a fault.
- WIN lines (counts) can be lost in the serial log, so missed and doubled counts are lower bounds; the ERR lines carry
  the timing.
- Results: https://github.com/esphome/esphome/pull/22912#issuecomment-7075772948 and
  https://github.com/esphome/esphome/pull/22912#issuecomment-7090987680
