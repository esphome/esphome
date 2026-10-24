# http_request

## Update manifest URL changed at run time (#21373)

`scripts/http_request/manifest_server.py PORT [DELAY]` serves `stable.json`, `beta.json` and `slow.json` (answered
after DELAY s) with both chip families and logs every request. `templates/http_request_update/upd-common.yaml`
(secret `manifest_base` is the server URL) has the update entity on the stable manifest, buttons for every
`set_source_url` shape (`std::string` local, literal, temporary, `.c_str()` of a local, the current URL back in),
`Check Then Swap` (ESP32 task path: check the slow manifest and replace the URL in the same lambda), `Arm Swap In
Check` plus a `logger: on_message` lambda that replaces the URL from inside the inline check (ESP8266; the log line
from inside `on_message` is swallowed, judge by the next check's result), and a `Heap` button. `upd-base-common.yaml`
keeps only the shapes that compile on a tree with the bare pointer setter (for the base build, copy a board file
and point its `common` include at `../upd-base-common.yaml` with a different `name:`); `boards/upd8266.yaml` and `boards/upd32.yaml` are the
per-board files, one directory below the package their `../` include points at. Drive with `scripts/http_request/updrive.py HOST KEY LOGFILE press:NAME wait:S`, which prints every
update state (latest version, summary). The ESP8266 Arduino client gets `Error reading manifest: -1` on roughly a
quarter of the fetches from this Python server on every build, so count checks against the base before blaming a
PR. `http_request` on the ESP8266 needs `verify_ssl: false` even for plain http URLs.
Results: https://github.com/esphome/esphome/pull/21373#issuecomment-6337544933

## Non-blocking request actions (#21486)

`scripts/http_request/hsrv.py PORT` serves `/fast`, `/slow?d=S`, `/big` and `/chunked` (`n` bytes at `rate` B/s,
deterministic bytes `(i*7+3)&0xFF`), `/post` (echoes length and byte sum), `/redirect` and `/hang`.
`templates/http_request_action/hr-common.yaml` (substitutions `name`, `bufsize`; per-board files `hr32.yaml` and
`hr66.yaml`) has API actions `req_get`, `req_post`, `burst` (N runs of a `mode: parallel` script on one action),
`cancel` (`mode: single` script run twice, then `script.stop`) and `reboot_during`, and logs
`RESULT n status len sum took maxgap`, where maxgap is the longest main loop gap during the request (an
`interval: 1ms` lambda plus the time since its last pass, so a fully blocking request counts too). Drive with
`scripts/http_request/hrdrive.py IP http://HOST:PORT LABEL [https]` (`ONLY=case,case` and `ROUNDS=n` env; the
`cross` case runs two actions on one component). `burstonly.py IP URL N` checks concurrency against the server's
completion times, `reboottest.py IP URL` times `App.safe_reboot()` with requests in flight, `updcheck.py IP LABEL N`
runs N update checks over the API (for real device configs that use `update: platform: http_request`) and
`otadrive.py` triggers `ota.http_request.flash` (the API variable is `sum`; `md5` is a reserved variable name).
`execute_service` must be awaited on current aioesphomeapi or nothing is sent. Keep the ESP32 `bufsize` at 32 KB or
less: the action copies the body into a `std::string` while the buffer is still held, and 80 KB aborts with out of
memory on dev and on the PR alike. `http_request` refuses BK72xx and RTL87xx at validation.

## OTA request cases with basic auth (#21556)

`scripts/http_request/ota_auth/`: `ho-common.yaml` (API actions for `ota.http_request.flash` with `md5_url`, `md5`
and basic auth, `flash()` with no URL, an invalid URL, the `md5_computed()`/`md5_expected()` getters), `fwsrv.py`
(serves a www dir, `/auth/` needs `otauser:otapass`), `hodrive.py` (11 cases, reconnects after each good flash) and
`run.sh BUILD BOARD` (serial flash, serves the build's own image; `TREE`, `P32` or `P66`, `SC` and `HOST_IP` in
the environment). On the D1 mini the authenticated MD5 read fails
with `Error reading MD5: -1` on dev too, and stored credentials carry over to the next flash on both builds.

Results: https://github.com/esphome/esphome/pull/21556#issuecomment-6381067561
