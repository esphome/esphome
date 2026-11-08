# IPv6 link-local and SLAAC on ESP32 Wi-Fi: boot timing and race injection (#22364)

`scripts/ipv6_boot/` (ESP32-S3 and ESP32-C3, native USB console, ESP-IDF). One YAML per build
(`example-s3.yaml`, `example-c3.yaml` show the shape) with the substitutions `min_count` (network
`min_ipv6_addr_count`) and `cap_ms`.

- `v6probe.hpp` and `v6common.yaml`: `network: enable_ipv6`, VERBOSE logs, and a 50 ms interval that logs milestones
  from boot (`MS assoc|ipv4|linklocal|slaac|connected <ms>`; addresses come from `esp_netif_get_all_ip6`, so they
  count once they are preferred, after duplicate address detection), then `RESULT boot ...` with `heap_used`, then
  disables Wi-Fi for 3 s and logs `RESULT reconnect ...` the same way, then reboots. A phase ends when SLAAC and the
  connection are both seen, or after `cap_ms` (`timeout=1`).
- `v6hf.yaml` adds a `HighFrequencyLoopRequester` (a busy main loop, as with a display or audio).
- `v6inject.yaml` links with `-Wl,--wrap=esp_netif_action_connected` and defines `V6_INJECT`; the wrapper in
  `v6probe.hpp` sleeps 300 ms in the event task before esp-netif brings the station interface up, after the wifi
  component already queued STA_CONNECTED. That makes the "interface not up yet" race happen on every connect.
- `run.sh NAME SECONDS` (env `TREE`, `PORT`, `SC`, `PY`) builds `NAME.yaml` as device `NAME`, flashes, captures the
  self-rebooting boots and runs `an.py CAPTURE...`: per phase the `esp_netif_create_ip6_linklocal failed` count,
  boots without link-local, SLAAC or a connection, connection timeouts, and median and worst times.

- Check the network first: a computer on the same network shows an `autoconf` IPv6 address when a router advertises
  a prefix. SLAAC arrived about 2 s after link-local on the test network.
- The natural race did not show: no failure in about 160 boots on the S3 (normal and busy loop) and the C3 without
  the fix. esp_event runs handlers in registration order and the wifi component registers before esp-netif, but the
  event task reaches esp-netif's handler within microseconds. Only the injected delay reproduced the failure.
- An extra event handler registered from `on_boot` cannot inject the race: it runs after esp-netif's handler, when
  the interface is already up. Wrap the esp-netif call instead.
- `wifi.disable` removes the addresses, so the reconnect phase creates link-local again on the same path.
- Count failures by the probe's `ll=0` too, not only by the warning: the USB console drops lines.
