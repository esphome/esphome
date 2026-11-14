# web_server Basic auth fuzz (async web server: ESP8266, RP2040/RP2350, LibreTiny)

- `authfuzz.py HOST USER PASS OUTPREFIX [--only cases|heavy|timing] [--ota IMAGE]`: raw-socket cases
  (functional routes with and without credentials, wrong credentials, schemes, tampered hash, spacing and
  case), then `heavy` (8 KB Authorization, 32/64 KB headers, 200 headers, 256 KB unauthenticated /update,
  600 failed attempts, 4 bad-credential threads next to an authenticated `/events` stream) and `timing`
  (wrong first byte vs wrong last byte, ms to the first 401 byte, Mann-Whitney z). Heap comes from the
  debug "Heap free" sensor over REST. `--ota` uploads the image with credentials and waits for the reboot.
  It is importable: `configure()` sets the host and credentials for `raw()`, `build()` and `uptime()`.
- `isolate.py HOST USER PASS [ROUNDS] [CASE,...]`: one hostile request at a time with the uptime before and after,
  to tie a reboot to one request.
- `compare.py DEV_PREFIX PR_PREFIX` lines the two runs up and prints both raw 401 heads.
- `configs/`: `wba-common.yaml` (web_server v3, Basic auth, web OTA, switch and light on top of
  `templates/diagnostics.yaml`), `creds-short.yaml` and `creds-long.yaml`, `d1.yaml` (D1 mini on the station
  network), `d1-ap.yaml` (captive portal DUT on `templates/dut-fallback-ap.yaml`), `ap-client.yaml` with
  `ap-client-common.yaml` (`templates/client-wifisave.yaml` plus probes of `/`, `/config.json` and a POST without
  and with credentials while on the AP), and `run.sh NAME CONFIG KIND PORT MODE` (build with the chosen
  credentials through `../pio_build.sh`, flash, capture, authfuzz, crash counts).

Library facts (ESPAsyncWebServer 3.9.6): each header line is trimmed before parsing, so trailing spaces or a
tab after the hash never reach ESPHome; the value skips one space after the colon only; a NUL byte in the
header aborts the request; the scheme split is at the first space, case-insensitive. Before the Basic auth rework,
`authenticate(hash)` compared the text after the first space whatever the scheme, so `Bearer <hash>`,
`Foo <hash>` and a bare `<hash>` were accepted.
