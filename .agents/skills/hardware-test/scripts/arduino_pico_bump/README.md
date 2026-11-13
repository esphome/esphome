# arduino-pico framework bump A/B (#22671)

Copy into a scratch dir with `secrets.yaml`, `cappico22671.py` (a copy of serial_capture.py), `apikey.txt` (the
`api_key` secret) and the `dut-fallback-ap.yaml`/`client-wifisave.yaml` templates. Make dev copies of the `*pr*.yaml`
files with another `name:`. `build.sh` and `up.sh` export a private `PLATFORMIO_CORE_DIR=<dir>/pio`; `build.sh` runs under
`$LOCK` when set. The scripts read `PY` (a checkout's venv python), `PICO_SN`, `PEER_MAC`, `CLIENT_PORT` and
`CLIENT_TREE` from the environment.

- `staflow.sh NAME TREE PICO_SN BLE`: USB flash, API soak with page loads, 30 s SSE, OTA, reboot, short soak.
- `bleseq.sh NAME [PICO_SN]`: 180 s GATT hold through the Pico W proxy (`gatthold.py`), then 4 short reconnects.
- `apflow.sh NAME TREE`: fallback AP on the Pico W, ESP32 client provisions it, logs the AP client count (`appeek.hpp`).
- `sseabc.sh HOST TAG`: SSE alone, with the API soak, and with page loads (per 5 s bins).
- `sserun.sh HOST TAG SECS MODE N`: N SSE streams with pcap, netstat poll, API log (`apilog.py`), probes after each; `pcapsum.py PCAP IP` summarizes.
- `churn.sh TAG SECS` (H=host): /events connect and disconnect churn with a crash report check; `stallctr.py`: atcp counters per stall.
- `soak.py HOST SECONDS LOG`: states, drops, uptime resets, reset reason, crash/assert lines from the API logs.
