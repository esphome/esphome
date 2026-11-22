# UART parity

## Telling 8N1 from 8E1 on the wire without a logic analyser (#22511)

A CH340 on macOS reports no parity or framing errors (boards.md, ESP8266), so read the board's TX at 115200 8E1
with `scripts/uart_parity/rd.py PORT SECS OUT e [ack] [feed]` and look at the data instead: bytes sent back to back
at 8E1 decode exactly, bytes sent at 8N1 lose step (the 8N1 stop bit is read as parity and the next start bit as the
8E1 stop bit) and give a fixed garble that `uartsim.py` (an edge-triggered 8E1 receiver model) predicts byte for
byte.

- `cal.yaml` calibrates the method: `Serial.begin()` at 8N1, then 8E1, the same pattern each time.
- An isolated byte with odd weight (0x7F) has the same waveform in both modes; prove parity with a back-to-back
  multi-byte write.
- `analyze.py CAPTURE` classifies the shelly_dimmer frames (exact 8E1, or matching the 8N1 model); `txt.py CAPTURE`
  prints the RX bytes as text. `ack` answers a lone 0x7F with 0x79 so stm32flash goes on to send GVR `01 FE`;
  `feed` writes 400 bytes after each report line to probe the RX buffer.
- `common.yaml` is a D1 mini with `shelly_dimmer` and a report line every 6 s (`par=` the UART parity, `c0=` the
  UART0 config register USC0(0), 0x1C = 8N1 and 0x1E = 8E1, `swap=` the IOSWAPU0 bit). The four variants:
  `sda.yaml` hardware UART0 on GPIO1/3, `sdb.yaml` swapped UART0 (GPIO15/13), `sdc.yaml` TX on GPIO1 with RX on
  GPIO14, `sdd.yaml` a software UART on GPIO12/14 (report on the logger only).
- A software UART with TX on GPIO1 and RX on another pin is bit-banged onto the USB adapter's RX, so software UART
  parity is visible too (logger on UART1).
- Swapped UART0 (GPIO15/13) on dev ended up back on GPIO1/3 after `Serial.begin()`; the report line shows it.
