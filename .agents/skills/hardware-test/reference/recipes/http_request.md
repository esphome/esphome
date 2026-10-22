# http_request

## Update manifest URL changed at run time (#21373)

`scripts/http_request/manifest_server.py PORT [DELAY]` serves `stable.json`, `beta.json` and `slow.json` (answered
after DELAY s) with both chip families and logs every request. `templates/http_request_update/upd-common.yaml`
(secret `manifest_base` is the server URL) has the update entity on the stable manifest, buttons for every
`set_source_url` shape (`std::string` local, literal, temporary, `.c_str()` of a local, the current URL back in),
`Check Then Swap` (ESP32 task path: check the slow manifest and replace the URL in the same lambda), `Arm Swap In
Check` plus a `logger: on_message` lambda that replaces the URL from inside the inline check (ESP8266; the log line
from inside `on_message` is swallowed, judge by the next check's result), and a `Heap` button. `upd-base-common.yaml`
keeps only the shapes that compile on a tree with the bare pointer setter; `boards/upd8266.yaml` and `boards/upd32.yaml` are the
per-board files, one directory below the package their `../` include points at. Drive with `scripts/http_request/updrive.py HOST KEY LOGFILE press:NAME wait:S`, which prints every
update state (latest version, summary). The ESP8266 Arduino client gets `Error reading manifest: -1` on roughly a
quarter of the fetches from this Python server on every build, so count checks against the base before blaming a
PR. `http_request` on the ESP8266 needs `verify_ssl: false` even for plain http URLs.
Results: https://github.com/esphome/esphome/pull/21373#issuecomment-6337544933
