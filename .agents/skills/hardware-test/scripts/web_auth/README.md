# web_server Basic auth fuzz (async web server: ESP8266, RP2040/RP2350, LibreTiny)

- `authfuzz.py HOST USER PASS OUTPREFIX [--only cases|heavy|timing] [--ota IMAGE]`: raw-socket cases
  (functional routes with and without credentials, wrong credentials, schemes, tampered hash, spacing and
  case), then `heavy` (8 KB Authorization, 32/64 KB headers, 200 headers, 256 KB unauthenticated /update,
  600 failed attempts, 4 bad-credential threads next to an authenticated `/events` stream) and `timing`
  (wrong first byte vs wrong last byte, ms to the first 401 byte, Mann-Whitney z). Heap comes from the
  debug "Heap free" sensor over REST. `--ota` uploads the image with credentials and waits for the reboot.
  It is importable: `configure()` sets the host and credentials for `raw()`, `build()` and `uptime()`.
- `isolate.py HOST USER PASS [ROUNDS] [CASE,...]`: one hostile request at a time with the uptime before and after,
  to tie a reboot to one request.
- `compare.py DEV_PREFIX PR_PREFIX` lines the two runs up and prints both raw 401 heads;
  it exits 1 when any case differs or exists on one side only.
- `configs/`: `wba-common.yaml` (web_server v3, Basic auth, web OTA, switch and light on top of
  `templates/diagnostics.yaml`), `creds-short.yaml` and `creds-long.yaml`, `d1.yaml` (D1 mini on the station
  network), `d1-ap.yaml` (captive portal DUT on `templates/dut-fallback-ap.yaml`), `ap-client.yaml` with
  `ap-client-common.yaml` (`templates/client-wifisave.yaml` plus probes of `/`, `/config.json` and a POST without
  and with credentials while on the AP; `approbe_check.py CLIENT_LOG` fails unless the POST without credentials
  got 401, the one with credentials 200 and every probe answered), and `run.sh NAME CONFIG KIND PORT MODE` (build
  with the chosen credentials through `../pio_build.sh`, flash, capture, authfuzz, crash counts).
- `leak.py HOST USER PASS [ROUNDS] [SETTLE_S] [CONC_S] [THREADS] [RATE]`: rounds of bad-credential load next to an
  authenticated `/events` stream, heap read after a settle each round. RATE paces the wrong-hash requests to RATE per
  second in total, so two builds that answer at different speeds get the same load (an unpaced flood compares load,
  not code).
- `upload_reject.py HOST USER PASS [N] [KB]`: unauthenticated and wrong-hash `/update` uploads with the status,
  uptime and heap after each.
- `rate.py HOST USER PASS [SEQ_N] [PAR_S] [THREADS]`: ms to the first byte and to the device's close per request,
  sequential and parallel. `pcapphase.py PCAP DEVICE_IP`: per-connection accept, respond and close times from a
  tcpdump capture (tcpdump drops packets at flood rates, so it samples).
- `nul_soak.py HOST USER PASS [N] [CASES] [MAXREB]`: NUL byte in the Authorization header, another header, the
  request line, and split across TCP segments; N requests per case with an uptime check after each, a case stops
  after MAXREB reboots.
- `configs/cpflow.sh DUT KIND DUT_PORT CLIENT_PORT [flashclient]`: flash the AP DUT, wait for "Starting fallback AP",
  only then reset the client, or the client joins the real network first and never sees the AP.
- `heap_churn.py HOST USER PASS [N] [RATE] [SAMPLE_S] [AFTER_S]`: free heap (the "Heap free" sensor over REST) while N
  wrong-credential requests run at a fixed RATE per second and for AFTER_S seconds after, sampled every 1 s for the
  first 20 s after the load; prints the idle heap, the lowest heap under load, how long the heap takes to come back
  within 2 KB of idle, and whether the uptime went backwards. `configs/msl.sh NAME bw15.yaml PORT` runs it with the
  big header cases on a BW15 (`configs/bw15.yaml` reads the heap every 1 s).
- `arena_drive.py HOST USER PASS [PHASE_S]` with `arena/`: the main task stack high-water mark under web_server load.
  `arena/stackmin.hpp` counts the untouched FreeRTOS fill bytes from the current task's stack base (0xA5 on the
  RTL8720C, 0x5A on BK7231), because LibreTiny compiles out `uxTaskGetStackHighWaterMark` and `vTaskGetInfo` on
  AmebaZ2; it finds `pxStack` either as the second TCB member (ARMv8-M with V8M_STKOVF) or right before
  `pcTaskName`. `arena/arena-common.yaml` exposes it and the heap as sensors; `arena_drive.py` runs idle, two
  `/events` streams, new clients every 3 s, select changes with a state over 1 KB, then all at once, and prints the
  lowest stack minimum per phase. `arena/arena.sh NAME bw15.yaml|bk7238.yaml bw|bk PORT` builds, flashes and drives it.

Library facts (ESPAsyncWebServer 3.9.6): each header line is trimmed before parsing, so trailing spaces or a
tab after the hash never reach ESPHome; the value skips one space after the colon only; a NUL byte in the
header aborts the request; the scheme split is at the first space, case-insensitive. Before the Basic auth rework,
`authenticate(hash)` compared the text after the first space whatever the scheme, so `Bearer <hash>`,
`Foo <hash>` and a bare `<hash>` were accepted.

lwIP TIME_WAIT on LibreTiny (#22696, TCP_MSL 250 ms): on a BW15 the base build lost 6 to 9 KB of heap through 600
connections and recovered 125 to 145 s after the load (2 x the 60 s default MSL); the PR stayed within 2 KB and
recovered in 1 to 2 s. Each heap reading is itself a connection, so polling after the load keeps lowering the base
heap. A BW15 CH340 that drops off USB makes the capture reopen the port, and every reopen resets the chip (Power-On);
check for "[capture] port lost" before blaming the firmware for a reset.

Main task stack with the JSON arena (#22699): the lowest free stack of the main task went from 5860 B to 3796 B on the
BW15 and from 5312 B to 3604 B on the BK7238 with the arena on (it costs about 2 KB of stack under load). Run
the BW15 without a serial capture at VERY_VERBOSE (`NOCAP=1`), for the same CH340 reason.

Findings from #22684 (ESP8266, Pico 2 W, BW15):
- A NUL byte in any header reboots the board on ESPAsyncWebServer 3.9.6: about 1 in 6 requests on the ESP8266
  (Exception 28/29, an OOM panic in `addHeader`), and nearly every request on the Pico 2 W (watchdog reset, no record).
  3.12.1 (#22682) fixes the ESP8266 to 1 in 800 (one Exception 29 in the Wi-Fi beacon code, a heap damage victim) but
  not the Pico 2 W. Rapid reboots drop the Pico into safe mode (web server off for 5 min), which a soak counts as
  reboots; cap the case.
- RP2 never closes the connection after a 401 although it says `Connection: close`; a client reading to EOF waits for
  its own timeout. Successful responses close after about 0.5 s. Same on dev.
- BW15: a 32 KB header or a 64 KB line with no newline resets the board (reset reason Power-On), and so does a 60 s
  4-thread flood; free heap drops about 60 KB over 600 rapid connections and stays low. Same on dev.
- ESP8266 web OTA uploads fail now and then on dev and PR alike (interrupted upload, or flashed without a response).
- Unpaced floods vary 2x between passes of the same build; compare builds in alternating passes (dev, PR, dev, PR)
  or paced. `/events` gets closed by the device (clean FIN after the ACK timeout) under an unpaced flood on any build.
- ESPAsyncWebServer 3.12.1 skips all leading spaces of a header value (3.9.6 skipped one).
