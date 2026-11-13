# web_server Basic auth fuzz (non-ESP32 async server: ESP8266, RP2040/RP2350, LibreTiny)

Used for the web-server-basic-auth-no-digest branch (b0ac6fe750). Still evolving when copied here.

- `authfuzz.py HOST USER PASS OUTPREFIX [--only cases|heavy|timing] [--ota IMAGE]`: raw-socket cases
  (functional routes with and without credentials, wrong credentials, schemes, tampered hash, spacing and
  case), then `heavy` (8 KB Authorization, 32/64 KB headers, 200 headers, 256 KB unauthenticated /update,
  600 failed attempts, 4 bad-credential threads next to an authenticated `/events` stream) and `timing`
  (wrong first byte vs wrong last byte, ms to the first 401 byte, Mann-Whitney z). Heap comes from the
  debug "Heap free" sensor over REST. `--ota` uploads the image with credentials and waits for the reboot.
- `compare.py DEV_PREFIX PR_PREFIX` lines the two runs up and prints both raw 401 heads.
- `configs/`: `wba-common.yaml` (web_server v3, Basic auth, web OTA, debug heap, 200 ms sensor, switch,
  light), `creds-*.yaml`, a per-board file, the captive portal DUT (`wbacp-*`, uses
  `templates/dut-fallback-ap.yaml`), the captive portal client package (`wbacpc-common.yaml`, probes `/`,
  `/config.json`, POST without and with credentials, a bogus and the real `/wifisave`), and `run.sh`
  (flash, capture, authfuzz, crash counts).

Library facts (ESPAsyncWebServer 3.9.6): each header line is trimmed before parsing, so trailing spaces or a
tab after the hash never reach ESPHome; the value skips one space after the colon only; a NUL byte in the
header aborts the request; the scheme split is at the first space, case-insensitive. Dev's
`authenticate(hash)` compares the text after the first space whatever the scheme, so `Bearer <hash>`,
`Foo <hash>` and a bare `<hash>` were accepted before the branch.
