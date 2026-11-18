# one_wire

## ESP8266 interrupt locks under Wi-Fi load (#22452, #22453)

Code that holds interrupts off for long on the ESP8266 starves the Wi-Fi interrupt handler (reset signatures in
`reference/boards.md`). `scripts/esp8266_lock_load/` is the shared harness for any component that takes such locks:

- `base.yaml` is the shared DUT base: reset reason and info logged at boot, `power_save_mode: none`, `web_server` for
  the HTTP load, `api: reboot_timeout: 0s`.
- `one-wire.yaml` reads a DS18B20 on GPIO4 every 2 s and runs a bus search every `search_interval`; `busy_control:
  "true"` replaces the search with the same busy time (13 ms) and no 1-Wire traffic.
- `ir-one-pin.yaml` is the remote_transmitter case (see [remote_base.md](remote_base.md)).
- `wifi_load.py HOST SECONDS PPS` sends 512 byte UDP datagrams to port 9 at a fixed rate plus back to back GETs of
  the web_server page.
- `run.sh` builds and flashes a DUT, captures the serial log, runs the load and prints every reset reason (usage in its
  header). A clean run has one BOOT line.

Make one file per build with `substitutions: {name: ...}` and `packages: {t: !include one-wire.yaml}`, with
`secrets.yaml` next to it, and give dev and the PR different names.

Notes:
- Run each build for 45 minutes at 500 packets per second. Shorter or lighter runs often show no reset on either
  build: one search every 5 s survived on both.
- The busy-wait control separates the lock from the CPU time. On #22452 it had no Hardware Watchdog in 45 minutes,
  while the PR still had 2 (dev had 6), so the remaining resets came from the slot locks. All three builds had one
  Software Watchdog, which is the busy time plus the load.
- A search with no sensor answering is much shorter than one with a sensor (about 190 slot locks per search with
  one DS18B20), so test with a real sensor on the bus.
- A stale `secrets.yaml` gives `No networks found` and the run has no IP to load.
- The 1-Wire bus code is shared by every platform, so check a lock change on an ESP32 too: the same DS18B20 search
  config on ESP-IDF with `bluetooth_proxy` scanning and the same load gave 0 resets and no missed searches on dev and
  on #22452.
