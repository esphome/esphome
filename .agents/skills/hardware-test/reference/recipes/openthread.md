# openthread

## Thread border router (#18104)

See boards.md for the ESP Thread BR board. Check that the H2 answers Spinel before blaming a PR.

## Border router flows (#22929, #16387)

- Reboot loop: an API restart button, then wait for the API and for the `omr` field of
  `dns-sd -L <name> _meshcop._udp` (a single dns-sd miss happens; retry). Backbone loss: a script runs
  `wifi.disable`, waits 30 s, then `wifi.enable`.
- Retry path: a test-only patch in a copy of the tree that makes the 2nd and 3rd
  `esp_openthread_border_router_init()` calls return ESP_FAIL; one Wi-Fi drop then shows two failures 10 s apart and a
  recovery.
- An ESP32-C6 as a native radio border router: Wi-Fi is slow under 802.15.4 coexistence (1 s pings, handshake
  timeouts), and OTA never completed on dev or the PR. Not a PR bug; flash it over USB.
- Child checks: a lambda on the border router logs `otThreadGetChildInfoByIndex` (timeout, age, supervision
  interval), one on the MTD logs `otThreadGetChildTimeout` and `otChildSupervisionGet*`. The parent shows supervision
  interval 0 while the child is rx-on-when-idle (OpenThread sends it only from a sleepy child). `core_options` are
  ESP32 only, so use a C6 MTD for those.
