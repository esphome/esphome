# bluetooth_proxy

## Held GATT link after a half-open API session (#21256, #22077)

When Home Assistant stops answering without closing its socket, the proxy must not keep the BLE link it opened for
that session. `scripts/bluetooth_proxy_half_open/` reproduces it with:
- `proxy.yaml`: the proxy under test on a classic ESP32 devkit (ESP-IDF, VERBOSE logger with the busy tags at DEBUG,
  encrypted API). Build it from the tree under test with a `secrets.yaml` (`wifi_ssid`, `wifi_password`, `api_key`).
- `peripheral.yaml`: a second ESP32 (an S3 here) as a peripheral that only advertises while nobody is connected
  (`esp32_ble_server` takes one client by default), with manufacturer data `FF FF 21 56` so the client can find it.
- `ha.py`: the Home Assistant stand-in (aioesphomeapi, `client_info` "Home Assistant test"). Roles: `owner` connects
  to the peripheral through the proxy and idles, `observe` counts the peripheral's adverts and the slot report and can
  ask for a connect at the end, `swap` connects at once and retries on the first advert. Every role stays subscribed
  to advertisements: with no subscriber the proxy's loop disconnects every link at once, which hides the bug (an
  early run failed with `disconnect while backend idle` for that reason).

`cases.sh PROXY_HOST LABEL` (env `PROXY_PORT`, `PERI_PORT`, `SC`, `PY`, `API_KEY`) captures both boards and runs
case C (old client killed, new client after 4 s), case H (old client frozen with `SIGSTOP`, so the socket stays open
with no FIN; new client after 4 s, watched 300 s, then it asks to connect and releases) and case N (frozen and nobody
reconnects for 200 s). `cases.sh PROXY_HOST LABEL swap` instead repeats H three times with the new client
connecting at once (`ha.py ... swap ADDR`) and retrying on the first advert. Both end with PASS or FAIL: the cases
fail when an observer exits non-zero, never sees the peripheral advertise (the held link) or the proxy resets, and
swap fails when a round never connects. An owner that never got the link also fails (a rig problem, not a result).

- The proxy drops a frozen session as unresponsive after about 144 s.
- Before #22077, case H holds the link for good: 0 adverts, and the address stays in the slot report. With #22077 the
  link is released in the same millisecond as the `replaces` warning. In case 4 the first request logs `Connection
  request ignored, state: DISCONNECTING` (connected=false at 0.2 s) and the retry connects at 0.5 to 0.7 s.
