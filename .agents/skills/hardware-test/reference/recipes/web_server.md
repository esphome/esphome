# web_server

## State SSE on web_server_idf (#21109) and the HTTPD task stack (#21038)

`templates/web-server-sse-dut.yaml` (ESP32 classic, ESP-IDF, `web_server` version 3) publishes eight sensors every
200 ms (small state events on the 1024 byte stack buffer) and three text sensors whose JSON carries the string twice
(`value` and `state`): 600 chars for about 1.3 kB (the 2048 byte tail), 1800 chars for about 3.7 kB (tail grown to
4096) and 2800 chars for about 5.7 kB, over the 5120 byte JSON room, which must log `State event does not fit`
and never put a truncated frame on the wire. An `interval` lambda logs `uxTaskGetStackHighWaterMark()` of the loop
task (`nullptr`) and of the HTTPD task (`xTaskGetHandle("httpd")`) every 5 s; the ESPHome loop task is `loopTask`
on ESP32. Keep `power_save_mode: none`, and drive the device by address: mDNS names dropped out mid-run once.

`scripts/web_server/sse.py HOST SECONDS [--clients N] [--stall] [--churn CYCLES]` opens raw TCP connections to
`/events`, decodes the chunked body and parses every `state` event as JSON; `--stall` adds a connection with a 2 kB
receive buffer that never reads (the device must log `Closing stuck stream` on ESP-IDF and `Closing stuck
EventSource connection` elsewhere, after the 20 s stall timeout), `--churn` opens and resets a connection CYCLES
times. `sse_cases.sh HOST KEY LABEL CAPTURE` runs the set: 1 client 60 s, 4 clients 60 s, a reader plus a stalled
client 45 s, a reader plus 10 abrupt closes, then two clients with the "Burst" button (50 sensor and 50 text
publishes in one loop pass). Pass: 0 invalid JSON on every client, the oversize sensor dropped with its warning on
every publish, the stalled session closed after about 20 s, no reset. The loop task watermark is a minimum since
boot, so compare the value after the first client, after four clients and after the burst between the builds (the
SSE split in #21109 left 1280 B more on the ESP32 classic).

For the HTTPD task, `web_server: auth: type: digest` (the default `auth` is Basic) and
`scripts/web_server/httpd_cases.sh HOST USER PASS LABEL CAPTURE`: `httpmix.sh` sends a `curl --anyauth` request
mix per round (page, `/sensor/<Name>`, `/text_sensor/<Name>`, `/switch/<Name>`, a `POST` with `Content-Length: 0`
to `/switch/<Name>/toggle`, an unknown path, which the IDF server closes without a response, an unauthenticated
request answered 401, and a 2 s `/events` stream), 20 rounds alone and 12 rounds next to two `ssecurl.sh` SSE
clients. Entity URLs use the entity name, URL encoded, not an object id. The HTTPD watermark follows the deepest
request of the boot; compare after the mix and after the SSE round, with two boots when the numbers are close.
Results: https://github.com/esphome/esphome/pull/21109#issuecomment-6300582328 and
https://github.com/esphome/esphome/pull/21038#issuecomment-6301310182

## Entity icon and device class readback (#21111)

Covers every path that reads an entity's icon or device class: the native API listing, MQTT discovery, the web_server
`/events` listing and REST endpoints, and `dump_config`. `templates/entity-icon-dut.yaml` (ESP8266 board with wifi,
encrypted api, ota, web_server and mqtt against a local broker) carries a sensor with a 63 character icon, one with no
icon or device class, an internal sensor, and binary_sensor, switch, button, text_sensor, number, select, event, cover
and text entities with icons and device classes. The per-run file supplies `name` and `broker_ip` through
substitutions. Flash the PR and base builds under different names, then from the scratch directory:

```sh
scripts/entity_icons/readback.sh DEVICE_IP KEY NAME BROKER_HOST cap-NAME.txt   # once per build
scripts/entity_icons/compare.sh ic-base ic-pr                                   # api, web, mqttn, dump all identical
```

`readback.sh` takes about 25 s once the device is on the network: 12 s of `mosquitto_sub` on `homeassistant/#` and
`NAME/#`, the API listing (`api_list.py`, one JSON line per entity with icon, device class, unit, options and event
types) and `web_dump.py` (6 s on `/events`, keeping only the listing rows that carry `domain`, then a GET of each
listed id with `?detail=all` and one retry, because the ESP8266 async server occasionally leaves a request unanswered
within 5 s). The web ids are `domain/Name`, not object ids, and `/event/<name>` has no GET handler (404 on every
build). `mqtt_norm.py` drops the `dev` block (compile time) and the `NAME/debug` topic (heap probe), so a clean run
differs in nothing. `dump_config` prints icons and device classes only for some domains; button, event and
binary_sensor icons never appear there.

Found with this flow on 2026-10-21: since #19577 every ESP8266 `web_server` build that lists a cover, climate, lock,
valve, update, alarm_control_panel or water_heater entity reboots with `Exception (3) LoadStoreErr` in
`json::linked(const char *)` (the ROM `strlen` on a flash address), fixed by #21316. The cover in the template keeps
that path covered; before #21316 is merged, apply its one line change to both trees or the web_server comparison
cannot run.

## Basic auth on the async web servers (ESP8266, RP2040, LibreTiny)

`scripts/web_auth/` (see its README) fuzzes Basic auth over raw sockets: functional routes with and without
credentials, wrong credentials, other schemes, a tampered hash, spacing and case, then large headers, many failed
attempts next to an authenticated `/events` stream, and a timing comparison of wrong first and last bytes. It also
has a captive portal client package that probes `/`, `/config.json`, POST with and without credentials and
`/wifisave`. `compare.py` lines a dev run up with a PR run. These scripts were still being extended when copied here.
The README also covers the newer flows: the heap under paced connection churn (`heap_churn.py`, the LibreTiny TIME_WAIT
check for #22696) and the main task stack high-water mark with the JSON arena (`arena/`, #22699).

## Per-session /events listing on ESP32 (#22691)

Checks that a new `/events` client's entity listing goes only to that client. Config
`scripts/web_server/sse-listing.yaml` (ESP32 ESP-IDF, web_server version 3, 19 entities, 3 sorting groups, heap logged
every 5 s); build each head with a different `-s name` through `pio_build.sh`. Driver:
`python -I scripts/web_server/sse_listing.py DEVICE_IP --rounds 4 --out events.jsonl`. Each round opens streams 1 and
2, waits for their listing, connects stream 3 and counts for 10 s per stream. A `state` event that carries `domain`
is a full detail (detail_all) entry; a `ping` with only `uptime` is the 10 s keepalive, any other `ping` the greeting
(config JSON). The greeting and `sorting_group` events always go only to the new session, so only the detail_all
count tells the builds apart. Ends `RESULT PASS` or `RESULT FAIL` (exit 0 or 1).

On an ESP32-PICO-D4 the control sent all 19 detail_all entries to streams 1 and 2 on every round, the PR none; stream 3
was ordered on both.

## /events stream rework checks (#21973)

Config `scripts/web_server/stream-dut.yaml` (ESP32 ESP-IDF, 20 ms template sensor, a "Wifi Drop" button that runs
`wifi.disable`, 8 s, `wifi.enable`, heap every 5 s). `scripts/web_server/stream_retest.py HOST MODE` has five modes:
`head` (raw bytes of the first 3 s; compare two builds with `head_compare.py A B NAME_A NAME_B`, which masks numbers
and the device name), `multi` (4 clients 20 s, events and longest gap), `stall` (a client with a 2 KB receive buffer
stops reading for 35 s; read the close time from `Closing stuck stream`, or `Closing stuck EventSource connection` on
older builds), `wifidrop` (REST press, the client reconnects after 5 s idle like a browser `EventSource`) and `churn`
(40 connects, 20 of them reset, then a fresh client). `stream_runall.sh NAME PORT` uploads, captures and runs every
mode. Both builds should close the stalled stream at about 20 s and close the stream at the Wi-Fi drop.

- `sse_listing.py` prints stream 3's TCP connect and first event times. A late greeting (3 to 4 s) was TCP connect
  time on the network (SYN retransmits), seen on the control too, not the device.
- On a noisy Wi-Fi network multi-client runs fell to 110 to 900 events per client with every client pausing together,
  and a stalled client sometimes missed the EOF although the device logged `Closing stuck stream`. Alternate the
  builds (flash A, 4 multi runs, flash B, repeat) before calling a rate difference.
- Log text check for refactors: collect every `[web_server*]` line from all captures, mask timestamps, source line
  numbers and digits, `sort -u`, and diff the two builds' sets.

## ESPAsyncWebServer / AsyncTCP bump A/B (#22682)

Scripts: `scripts/web_server/async_bump/` (README there). ESP32 never uses ESPAsyncWebServer: `web_server_base` loads
`web_server_idf` for every ESP32 build, Arduino included, so only ESP8266, RP2040/RP2350 and LibreTiny need a run.
Per board: page loads, a 180 s SSE client, REST control, `/update` OTA with the build's own image, restart button,
then the captive portal hammer (`scripts/wifisave_hammer.sh`, `bogus_saves: "5"`) on one board per family.
- Library check: ESP8266 builds use the native toolchain; the library copies sit in
  `.esphome/pio_components/arduino8266/<hash>/esp32async/*/library.json`, not in `.piolibdeps`.
- Restart button over REST on LibreTiny: the HTTP reply is often lost (curl 000 after 5 to 10 s) although the board
  restarts, on dev too. Not a regression signal.
- LibreTiny AsyncTCP 3.5.0 does not build with `framework: loglevel: debug` (or more verbose): `AsyncTCP.cpp:332` logs
  `xPortGetCoreID()`, undeclared on LibreTiny; `log_d` is empty at the default level, so normal builds pass. Any
  library bump for LibreTiny needs one build with the framework loglevel at debug.
- SSE rates on a board with a weak radio (a CB3S, say) are unreliable; decide SSE questions on a board with a good
  link and the same SDK.
- `/events` churn (`rp2_web_ble/churn.sh`) faults `AsyncEventSource::send` in `AsyncClient::connected()` from
  `deferrable_send_state` on the Pico W and the W5500-EVB-Pico; 3.12.1 and the RPAsyncTCP fork do not fix it.
- Stalled-reader runs need about 150 s: the client's receive window can take about 60 s to close, so a 60 s stall run
  ends before the 20 s device timeout even starts.

## JSON output A/B and the stack smoke (#22699, #22695)

- `scripts/web_server/json_ab.py HOST OUT` records every JSON document web_server builds: the `/events` ping, sorting
  groups and listing entries, `GET /<domain>/<name>` with and without `?detail=all`, and the state events of 9
  commands sent after resetting each entity to a known state. Live values are masked and keys sorted, so a `diff`
  between builds (after replacing the device names) must be empty; it exits 1 on any document that does not parse.
  Entities: `scripts/web_server/json-ab-entities.yaml` (13 entities in 12 domains; add a `web_server: sorting_groups:`
  entry with id `grp_a`), plus a template sensor named `Fast counter`. Run it on a fresh boot of each build, so run 1
  of every build starts from the same states.
- The JSON arena (`USE_JSON_ARENA`) is used only by `web_server/event_source.cpp`; REST documents never use it, so
  they are a control within each run.
- `sse_listing.py` also reads responses that are not chunked (ESPAsyncWebServer on RP2 and LibreTiny before #22695
  sends raw SSE). On a dev RP2 build the listing is already per session, so the check only separates builds on
  ESP-IDF.
- Smoke after a change that only merges dev: greeting order, log text comparison and two churn rounds per build. Dev on
  the BK7238 locked the main loop in the first churn round and then aborted in `operator new` from
  `AsyncWebServerResponse::addHeader` (a build with the LibreTiny abort handler reboots instead of hanging).

## /events churn with observers, REST and HTTPS 501

`scripts/web_server/churnmix/` (`s3.yaml` per board file on `common.yaml`; `commonlite.yaml` drops the 20 ms sensor
for slow boards): `churnmix.py HOST --cycles 400 --json OUT` keeps 2 observers on `/events` for the whole run, sends
REST every 250 ms (GET sensor, GET switch, POST toggle, the page), then runs 4 workers through 7 close patterns (read
to the first state, RST mid stream, RST or close right after the request, half a request line, full 2 KB receive
buffer then RST, read 1 to 3 s), a settle phase, the final relay state against each observer, and 4 fresh clients. The
`Counter` sensor counts up every 100 ms, so skipped (coalesced) and repeated values show; `--no-rest --no-post
--cycles 0 --observers 1 --base 60` is a quiet single client rate run. `fin_first` failure reasons are in the JSON
(`eof_nohead` means the device closed with no HTTP head). `summ.py NAME...` summarises `cm-NAME-1.json`,
`cm-NAME-2.json` and `q-NAME.json`. Ends `RESULT PASS` or `RESULT CHECK` with the reasons.
- Run one board at a time: parallel churn on two boards gives observer reconnects and half the events on both.
- Repeated values are by design: `process_deferred_queue_()` builds the queued event from the current state, then the
  new event goes out too. Coalescing on ESP8266 and RP2 comes from writing only after the last send is acked.
- HTTPS: `web_server` with HTTPS is refused at validation. `ext/components/https_test` (used by `s3https.yaml`) is a
  bench copy that patches `web_server_idf.require_plain_http` for `web_server` and sets port 443 and the certificate in
  its constructor (calling `init()` in `setup()` is too late once `web_server` started the server on port 80). It
  includes `certs.h`: run `scripts/https_idf/make_certs.sh` and copy its `ext/httpstest/certs.h` next to
  `https_test.hpp`. `httpscheck.sh HOST [ROUNDS]` expects 501 on `/events` and 200 on REST and the page.
- Findings: #22695 on the Pico 2 W closed about 40 % of the waiting churn clients with no response (the head was not
  queued in `StreamHandoffResponse::_respond`). Dev locks the BK7238 main loop after about 150 s of churn and takes the
  free heap of an ESP8266 down to 6 kB.
