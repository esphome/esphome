# tcp packet_interface link (#21119)

The `tcp` link carries `packet_interface` packets over one TCP connection: 2 byte little endian length, then the
packet; a zero length packet is a keep-alive. Most of it can be tested with no board: the component works on `host:`,
so two host builds on one computer talk over 127.0.0.1, and a raw Python peer covers what ESPHome cannot do on purpose.

Files in `scripts/tcp_link/`:

- `tcplink.yaml` (package): per device an `et_srv` and `et_cli` link carrying `entity_transport` both ways over xxtea
  plus fragmentation (nine 191 char broadcast ids so the dictionary is larger than one 1022 byte TCP frame), a
  `raw_srv`/`raw_cli` pair with no layers running a size sweep up to 1022 with checked payloads and sequence numbers
  (`STAT` line every 10 s: tx, refused, rx, bad, gap), a `probe_srv` (with `allowed_ips`) and a `probe_cli` for the
  Python peer, `CONN <link> 0/1` from every `connected` sensor, `ETRX` lines for remote states, and a 1023 byte send
  20 s after boot (must be refused). API on `${api_port}` with an encryption key from secrets.
- `host-peer.example.yaml`, `esp32-peer.example.yaml`: the per device substitutions (ports, peer host, allow list).
  The ESP32 one adds `deny_srv` with an allow list that excludes the test computer, for the refused connect.
- `tcp_peer.py client HOST PORT | server PORT`: raw peer. `--silent-after S` stops sending (receive timeout and
  reconnect), `--oversize-after S N` sends a header of N bytes (must close), `--send 1,1022` sends checked sizes,
  `--bind ADDR` picks the source address, `--hold S` closes after S. Prints keep-alive gap min/max per connection.
- `apiwatch.py HOST PORT SECS` (`API_KEY` from `apikey.txt`): state changes with timestamps from connect; shows the `connected` sensors
  read False (not unknown) at boot. The API coalesces a fast off/on, so judge edges from the `CONN` log lines.
- `run_pair.sh host|esp32 [RUN]`: the timeline (boot states, SIGSTOP/SIGCONT of the host peer, kill -9 and restart,
  silent and oversize raw servers, one client at a time, a refused client, oversize from a client) against host build
  `tcph1` (peer `tcph2`, refused client from the LAN address; `LAN` overrides the macOS `ipconfig` guess) or the ESP32
  `tcpe1` (`NAME` overrides it, found with `resolve.sh`; peer `tcph3`, refused client on `deny_srv`). Build the host
  configs here first. Ports default to the example configs (`API_PORT`, `PROBE_PORT`, `PROBE_CLI_PORT`, `DENY_PORT`).
  Logs: `<RUN>-dut.txt` (host mode), `-peer-a.txt` and `-peer-b.txt` (before and after the kill), `-api.txt`, the raw
  peers and `-events.txt`, without ANSI colours. It exits 1 when a job ends with the wrong status (`apiwatch.py` fails
  on a dropped API connection, `tcp_peer.py` on no connection or a missed close), a device log shows bad raw packets or
  an accepted 1023 byte send, or (host mode) a size the raw clients sent never reached `probe_srv` with its sum.
- `validation_cases.py`: writes one config per validation rule into `out-valid/`, named ok_ or bad_ by the expected
  outcome; run `esphome config` on each from the tree under test.

Facts:

- Source address trick: a server with `allowed_ips: 127.0.0.0/8` accepts `127.0.0.1` and rejects a connect to the
  computer's own LAN address with `--bind <LAN address>` (`Rejected <address>` in the log). No second
  machine needed.
- A server accepts one client; the kernel completes a second client's handshake (backlog 1), the second gets nothing
  until the first closes, then it is accepted and its queued packets are delivered.
- SIGSTOP of a host peer: the other side closes after `timeout`, the client reconnects after `reconnect_interval` and
  connects to the stopped process's backlog, then times out again. kill -9 closes at once (EOF/RST).
- One packet accepted by the socket just before a drop can be lost (the raw sweep shows one sequence gap per
  direction per drop); the session goes down and `entity_transport` resends everything on reconnect, so states are
  not lost.
- Host builds bind every interface; give each host build its own API port and link ports (6053 may be taken).
- ESP32 pair (`run_pair.sh esp32`): the host peer reaches the board by its mDNS name; set `HOST_LAN_IP` in the ESP32 example to the computer's address.
  Over Wi-Fi with `power_save_mode: none`, the raw peer saw keep-alive gaps of 1.65 to 2.26 s. A hardware reset (`esptool --after hard_reset chip_id` with the capture stopped) sends no FIN, so the host closes
  on its own receive timeout and then reconnects.
