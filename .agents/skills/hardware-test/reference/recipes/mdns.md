# mDNS responder (#22267)

`scripts/mdns/mdns-common.yaml` (a package) adds a `_hwtest._tcp` service with a short and a 62 character TXT
value and logs free, minimum and largest free heap every 10 s; `mdns-wifi.yaml` adds Wi-Fi from `secrets.yaml` and
switches it off and on every 120 s so the responder restarts. The host's multicast often does not reach the boards
(a host with several network interfaces, or a mesh Wi-Fi network), so the scripts send legacy unicast queries (source port not 5353)
straight to the device's port 5353 with python-zeroconf packets:

- `mq.py IP HOSTNAME dump` asks for the `_esphomelib` and `_hwtest` PTR records, the A record and the SRV and TXT
  records of both services, prints the answers and exits 1 when one is missing. Diff the dump between builds.
- `mq.py IP HOSTNAME hammer N [TIMEOUT]` sends N questions back to back (0.5 s timeout each) and counts misses per
  question. It exits 1 when nothing was answered, or when more than `MAX_MISS_PCT` percent (env) were missed.
- `mq.py IP HOSTNAME lat N` sends A queries 300 ms apart and prints the median, p90, p99 and maximum latency and how
  many answers came after 1.5 s or never.

Judge responder timing on an Ethernet board: on a crowded Wi-Fi network back-to-back query loss swung between 0 and
14% between builds for no code reason. Run the hammer across a Wi-Fi off and on cycle too, and compare the heap
lines before and after many cycles.
