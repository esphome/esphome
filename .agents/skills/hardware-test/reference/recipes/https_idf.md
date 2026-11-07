# HTTPS on web_server_idf (#22185)

#22185 adds an HTTPS developer API to `web_server_idf` with no YAML option, so `scripts/https_idf/` brings a test
component: `ext/httpstest` (`httpstest.hpp`) serves `/hw` (a status line with mode, request count and free heap),
`/hw/big` (16 KB) and `/hw/switch/{http,https,https8443,bad}`. A switch runs on the main loop through `defer()`, or
3 s later with a `/later` suffix (a stalled TLS client holds the server task, so its switch is armed first), and
calls `set_ssl_credentials(cert, key, port)`: plain HTTP on 80, HTTPS on 443 or 8443, or a key that does not match
the certificate. Its schema uses `web_server_idf.consume_ssl_sockets`. The run-time switch is the follow-up API; on a
tree that only has #22185, drop the switch paths and set the credentials once before `init()`.

Copy the directory into a scratch directory with a `secrets.yaml`, then `run.sh LABEL` (env `TREE`, `PORT`, `SC`,
`PY`): it runs `make_certs.sh` (a self-signed P-256 certificate, its key and an unrelated key in
`ext/httpstest/certs.h`; test material, never commit it), builds `https-dut.yaml` (ESP32-C3, logs on USB), flashes,
takes the address from the serial log and runs:
- `t1.sh HOST`: one GET with the TLS time, 20 sequential, 6 parallel three times and the 16 KB body. The fixture has
  no event source, so the 501 that #22185 sends for an event stream on HTTPS is not covered here.
- `stall.py HOST`: arms a switch to HTTP and to 8443, opens two idle TCP clients (no ClientHello) on 443 before it
  runs, then switches back; every switch must be accepted and work, and the idle clients must be closed.
- `t2.sh HOST`: HTTP and back, an armed switch to 8443 with a client open that sends a handshake record header and
  never the rest, 60 requests that must all get a 200 for a heap check (compare the `/hw` heap before and after),
  then the bad key. The server must log the error and stay off, without falling back to plain HTTP; reset the board
  afterwards.

The runner fails on any unexpected status, a missing key error for the bad key, a reset or a task watchdog. `https-soak.yaml` adds a switch between 443
and 8443 every 20 s for a long run with clients attached. Each HTTPS connection takes about 26 KB of internal RAM
(no PSRAM on the C3), so watch the `ALIVE heap=... min=...` lines.
