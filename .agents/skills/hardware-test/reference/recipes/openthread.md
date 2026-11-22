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

## Restart race flows (#22929)

`scripts/openthread_race/race.yaml` (with `wifi-drop.yaml`) drives a race against "border routing restarts": a
`logger: on_message` lambda that matches the line runs, after 0 to 500 ms, `wifi.disable` for 20 s or
`App.safe_reboot()`, or calls `wifi::global_wifi_component->disable()` inside the log callback itself. The API
action `race(mode, delay_ms)` arms it and starts each case with a 15 s Wi-Fi drop. Add `race-auto.yaml` to run all
37 steps on the device (a step counter in a restored global) when the API is too slow, as on a C6 under coexistence.

- The OpenThread task always started the router before `loop()` saw the Wi-Fi loss, even with the disable inside
  the restart log callback, on the PR and on dev: the dropped start path is not reachable this way.
- A C6 left with OpenThread state from another build did not bring Thread up (no NAT64, no `_meshcop._udp`), on old
  and new builds alike; `esptool erase-flash` fixed it.

## Ethernet backbone on the ESP32-S31 core board (#22929)

`s31-br.yaml` is an Ethernet-only border router (`openthread: border_router: backbone: ethernet`, native radio, no
`wifi:`; S31 Ethernet facts in boards.md), with the dataset in `secrets.yaml`. A C6 as an FTD end device on the same
dataset (erase its flash first), with an `interval:` lambda logging `get_ip_addresses()`, gives its OMR address:
`ping6` it from the host, `traceroute6` shows the S31 Ethernet address as hop 1, its `.local` name resolves through
the SRP advertising proxy, and the API works over Thread.

- `rb.sh NAME COUNT OUT [TARGET6]` restarts the border router through its Restart button COUNT times and waits for
  the API, the OMR prefix and a ping6 to the end device. The OMR prefix comes from the device: `omr.yaml` adds an
  `OMR Prefix` text sensor and `omr_state.py` reads it over the API. The `meshcop.sh` TXT record can come from the
  host's mDNS cache and predate the restart.
- Software link loss that looks like a pulled cable: `linkdrop.yaml` with `phy.hpp` sets BMCR bit 11 (power down)
  through `esp_eth_ioctl(..., ETH_CMD_WRITE_PHY_REG, ...)` and later clears it with bits 12 and 9 (autonegotiation
  enable and restart). The driver logs `Ethernet backbone lost` within 1 to 2 s. Read the recovery times from the
  device log; a host-side ping to detect the return was not reliable.
- Dev control: the merge base rejects any S31 border router config (the native radio needs Wi-Fi there) and the
  `backbone:` key.
- Shutdown logs `update_meshcop_mdns(52): Failed to update Openthread MeshCop TXT data`, also on Wi-Fi builds and dev.
