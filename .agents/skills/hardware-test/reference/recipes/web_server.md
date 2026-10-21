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
receive buffer that never reads (the device must log `Closing stuck EventSource connection` after the 20 s stall
timeout), `--churn` opens and resets a connection CYCLES times. `sse_cases.sh HOST KEY LABEL CAPTURE` runs the
set: 1 client 60 s, 4 clients 60 s, a reader plus a stalled client 45 s, a reader plus 10 abrupt closes, then two
clients with the "Burst" button (50 sensor and 50 text publishes in one loop pass). Pass: 0 invalid JSON on every
client, the oversize sensor dropped with its warning on every publish, the stalled session closed after about
20 s, no reset. The loop task watermark is a minimum since boot, so compare the value after the first client, after
four clients and after the burst between the builds (the SSE split in #21109 left 1280 B more on the ESP32 classic).

For the HTTPD task, `web_server: auth: type: digest` (the default `auth` is Basic) and
`scripts/web_server/httpd_cases.sh HOST USER PASS LABEL CAPTURE`: `httpmix.sh` sends a `curl --anyauth` request
mix per round (page, `/sensor/<Name>`, `/text_sensor/<Name>`, `/switch/<Name>`, a `POST` with `Content-Length: 0`
to `/switch/<Name>/toggle`, an unknown path, which the IDF server closes without a response, an unauthenticated
request answered 401, and a 2 s `/events` stream), 20 rounds alone and 12 rounds next to two `ssecurl.sh` SSE
clients. Entity URLs use the entity name, URL encoded, not an object id. The HTTPD watermark follows the deepest
request of the boot; compare after the mix and after the SSE round, with two boots when the numbers are close.
Results: https://github.com/esphome/esphome/pull/21109#issuecomment-6300582328 and
https://github.com/esphome/esphome/pull/21038#issuecomment-6301310182
