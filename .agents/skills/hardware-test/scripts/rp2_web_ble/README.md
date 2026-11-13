# RP2040/RP2350 web_server, API and BLE proxy A/B (#22671)

Configs take their name from the command line (`-s name ...` through `../pio_build.sh`), so a PR build and a dev build
of the same file never share a build directory: `picow.yaml` (Pico W with an active bluetooth_proxy), `pico2w.yaml`,
`evb.yaml` (W5100S-EVB-Pico on Ethernet), `picow-ap.yaml` (fallback AP with the AP client count from `appeek.hpp`) and
`client.yaml` (ESP32 that provisions it). `rp-common.yaml` adds the encrypted API, web_server and a restart button to
`templates/diagnostics.yaml`. Put `secrets.yaml` and `apikey.txt` (the `api_key` secret) in this directory; both are
git-ignored.

The runners read `TREE` and `PY` from the environment, plus `PICO_SN` (the Pico's USB serial number), `PEER_MAC` (the
`ble_notify/server.yaml` GATT server) and `CLIENT_PORT`/`CLIENT_TREE` where noted. Each ends with PASS or FAIL and
exits 1 on FAIL.

- `staflow.sh NAME CONFIG BLE`: USB flash, API soak with page loads, 30 s SSE, OTA, restart, short soak, and with
  BLE=1 a 180 s GATT hold.
- `bleseq.sh NAME`: 180 s GATT hold through the Pico W proxy (`gatthold.py`), then 4 short reconnects.
- `apflow.sh NAME`: fallback AP on the Pico W, the ESP32 client provisions it, the AP client count is logged.
- `sseabc.sh HOST TAG`: SSE alone, with the API soak, and with page loads (state events per 5 s).
- `sserun.sh HOST TAG SECS MODE N`: N SSE streams with a pcap, a netstat poll, the API log (`apilog.py`) and probes
  after each; `pcapsum.py PCAP IP` summarizes the pcap and `stallctr.py PCAP IP APILOG` lines the RPAsyncTCP counters
  up with each stall.
- `H=<device ip> churn.sh TAG SECS`: `/events` connect and disconnect churn, reboots from the reported uptime, then
  the crash report through `../crash_report/peek.py`.
- `soak.py HOST SECONDS LOG`: states, drops, uptime resets, reset reason, crash and assert lines from the API logs.
