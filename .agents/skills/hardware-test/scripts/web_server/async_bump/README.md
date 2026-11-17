# ESPAsyncWebServer / AsyncTCP bump A/B (#22682)

Put `secrets.yaml` here and per-board configs that include `aws-common.yaml` (web_server, web_server plus esphome OTA,
prometheus, encrypted API, a 200 ms counter, uptime every 1 s, heap, template switch and number, a monochromatic light
on a template output, a restart button). The runners use `./pio` as the private PlatformIO directory; build with
`../../pio_build.sh compile NAME NAME.yaml` from the tree under test first.

- `run.sh NAME KIND PORT` (KIND esp, pico, bk, bw; env TREE): serial flash, capture, `flow.py`, `webota.sh`,
  crash and boot counts.
- `otarun.sh NAME IP`: the same for boards reached only over the network (native OTA first, API log capture instead
  of serial).
- `flow.py HOST OUT [PAGE_S] [SSE_S]`: 36 rounds of `/` plus a REST GET or `/metrics`, a 180 s `/events` client
  (state count, device closes, gaps over 3 s), REST control (switch toggle, number 42, light brightness 128 read back).
- `webota.sh HOST IMAGE OUT`: `/update` upload (firmware.bin, or the UF2 on LibreTiny), wait for the uptime reset,
  restart button, heap after reboot.
- `ssediag.py HOST SECONDS [PAGE_S]`: SSE with a REST poll every 10 s; per window fast-counter events and the lag
  between the last SSE value and the REST value. A stall shows as `fast events 0` with a growing lag.
- `presstest.sh IP`: restart presses at uptime 20 s and 60 s, HTTP code and whether it restarted.
- `bkab.sh BUILD...` (env PORT): alternating serial-flash passes on a BK72xx board with a pcap of port 80 per pass.

## ESP8266 web OTA cases

- `ota8266.yaml` + `build.sh dev|pr a|b`: two images per build that differ only in a version text sensor
  (`-s ver a/b`), copied to `img-<kind>-<ver>.bin` here. `TREE_BASE` picks `<base>-dev` and `<base>-pr`.
- `otacases.py HOST KIND OUT [REPS] [1|2|3]`: case 1 full `/update` uploads alternating images (OK, REPLY_LOST,
  INTERRUPTED, BAD), case 2 raw multipart POST cut at 30% and 80% (reboot, serving, heap), case 3 upload with a held
  and a reconnecting `/events` client (a stream counts as ended on FIN or RST, or 6 s of silence).
