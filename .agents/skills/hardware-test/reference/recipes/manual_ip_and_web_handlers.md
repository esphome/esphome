# Manual IP and web handler flows (#21708, #21709, #21711, #21712)

## Station manual IP (`scripts/manual_ip/run-sta.sh`)

Set `static_ip` to the board's own DHCP lease so nothing collides, and DNS2 to a server DHCP does not hand out
(9.9.9.9), so the dump and `dns_getserver()` prove the manual values are in use. A missing SSID with its own
`manual_ip` comes first, so the real network is a later table row and inherits the top level `manual_ip`. The probe
logs both DNS servers and resolves a name every 20 s; `ipaddr_ntoa` uses one static buffer, so it uses
`ipaddr_ntoa_r` to log two addresses in one line. PASS needs the address, both DNS servers, a lookup and a ping.

## AP manual IP (`scripts/manual_ip/run-ap.sh`)

The DUT has only a missing SSID, `ap_timeout: 10s` and `captive_portal:`, so its AP comes up on 10.42.7.1; the other
board joins as a client and fetches `http://10.42.7.1/` every 10 s. A D1 mini AP drops some client requests on dev
too, so compare failure counts with a dev run and do not expect zero.

## Ethernet manual IP

Same idea on an Ethernet board: its lease as `manual_ip` plus an outside DNS2 and the DNS probe from
`scripts/manual_ip/dnsprobe.hpp`; restart it over the API and check again.

## Web handlers (`scripts/web_handlers/run.sh`)

`webbase.yaml` adds a GET /probe handler from `on_boot` at priority -100 (the shape of an external component adding
a handler late) and takes a variant: `ws` (web_server), `cp` (captive_portal only, started on the station network by
a button calling `global_captive_portal->start()`) or `prom` (captive_portal and prometheus). The runner fetches
/probe and /, then uploads the same image through web OTA (`curl -F update=@<bin> http://ip/update`; ESP-IDF
`build/<name>.bin`, not `firmware.ota.bin`; ESP8266 `.pioenvs/<name>/firmware.bin`) and checks over the API that the
board is back on that build. On dev too, prometheus GET / gets no answer on ESP32 and 404 on ESP8266.

A background `(cd dir && cmd) &` does not change the parent shell's directory: give capture scripts absolute paths.
