# serial_proxy with a simulated device (#22687)

`scripts/serial_proxy/`: no external device. On an ESP32-C3 (logger on USB_SERIAL_JTAG) the proxy UART is tx 4 rx 5
and the simulator UART tx 5 rx 4 (GPIO matrix, `allow_other_uses` on all four pins). `spsim.hpp` is the device; the
client drives it through the proxy with 7E-framed commands: `R` one 88 byte reply (A5, seq u32 BE, 83 bytes
(seq + j) % 251), `S` stream replies back to back, `A` ASH DATA frames every 50 ms with ACK latency timing (for
`ezsp_proxy_tap` in PROTOCOL mode), `X` log `SIMDONE` totals and reset, `B<baud>` change the sim's baud. SIMSTAT and
ASHWIN lines each second carry `backlog_max`, the proxy UART's RX backlog (bytes held back).

- `spclient.py HOST --baud B --mode reply|stream|ash [--interval MS] [--duration S | --pauses N --pause S --read S]
  [--protocol]`: raw plaintext API client (no encryption in the config), subscribes, brings the sim and the proxy to
  the baud, pauses reads on a schedule and prints a RESULT line (complete and broken replies, received bytes). Compare
  `received` with the sim's `SIMDONE bytes`.
- `suite.sh BUILD TREE` (env PORT) flashes `spBUILD.yaml` (a copy of `example-run.yaml` with its own `name`),
  captures, runs every case (`CASES="a b"` picks some).
- `rx_buffer_size` on the proxy UART decides how long a blocked client can stall before the UART overflows
  (32768 used; at 115200 a 5 s pause peaks near it).
- macOS ignores a small SO_RCVBUF after connect (reads back 36000), so a pause needs a few seconds to back up.
- Dev at 1920 baud loses bytes even with the client reading: the proxy sends 1 byte API messages (the UART wakes
  the loop per byte) and the TCP queue fills; #22687's wait coalesces them. A tap in PROTOCOL mode keeps that loss.
- Stream mode at 115200 (11.5 KB/s) is more than the proxy forwards over Wi-Fi; both builds lose there.
