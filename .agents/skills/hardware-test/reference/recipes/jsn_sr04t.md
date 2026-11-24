# jsn_sr04t

## SR04M-2 / AJ-SR04M on an ESP8266 (#21874)

The module's mode depends on the resistor at R19, and the sources disagree on the values. The Probots AJ-SR04M
sheet: 120k automatic serial, 47k sleeps until `0x01`, 300k low power, 0R continuous. A reported SR04M-2 streamed by
itself with 47k and answered the `jsn_sr04t` `0x55` request with 120k. Check what the board in hand does before
judging a driver.

- R19 empty is trigger/echo (HC-SR04) mode: no serial bytes on their own or for `0x55`/`0x01`, but every byte
  written on the trigger pin starts a ping, and the echo pin (idle low) shows one HIGH pulse (2.67 ms at about
  46 cm). The `ultrasonic` platform read 40/40 with `pulse_time: 10us`.
- One SR04M-2 at 3.3 V with 120k streamed `FF HI LO SUM` (the sum includes FF) about every 98 ms by itself and
  ignored `0x55`/`0x01`; bytes 1144 us apart, the idle pin rests HIGH. `jsn_sr04t` read it fine.
- A capture with no bytes at all was a loose header pin, not the mode: reseat the jumpers before blaming the R19
  value or the 3.3 V supply.

`scripts/esp8266_edges/` (ESP8266, trigger/RX on GPIO14, echo/TX on GPIO12; the `hz*` files share `hz-base.yaml`):
- `edges.hpp`: timestamps every GPIO12 edge in an ISR and dumps the intervals each second. `hzedge.yaml` writes
  0x55, then 0x01, on a TX-only UART with the edge logger running; `decode.py CAPTURE` decodes 9600 8N1 bytes from
  the dump. Use it to tell "no signal" from "the decoder drops bytes".
- `hzprobe.yaml`: the raw `uart` debug listener (15 s silent, 20 x 0x55, 20 x 0x01, silent).
- `hzjsn-common.yaml`: `jsn_sr04t` at 1 s with a reading counter logged as `TALLY`. `count.sh CAPTURE` counts
  requests, bytes, readings and errors up to the 61st request; `stream.py CAPTURE` splits the logged bytes into
  frames and stray bytes.
- `run.sh YAML NAME [SECONDS]` (env `TREE`, `PORT`): build, serial flash and capture from reset.
