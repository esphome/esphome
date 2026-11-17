# espnow

## ESP-NOW on the ESP8266 (#14407)

The NONOS SDK only delivers broadcast ESP-NOW frames while the station is associated or a soft-AP is
up; a hidden, open, client-less soft-AP (60 s beacon, DHCP stopped) makes broadcasts arrive. In AP-only
mode the device transmits from the soft-AP MAC.

## ESP-NOW and UDP packet_interface links (#21120)

Files in `scripts/espnow_link/` (a D1 mini and a classic ESP32; copy each `*.example.yaml`, set `name`, the peer's name
and its MAC or address):

- `enlink.yaml` + `en32.example.yaml`/`en66.example.yaml`: ESP-NOW without Wi-Fi on a fixed channel. Links `p2p`
  (peer_address, xxtea, hdlc, session, fragmentation), `bc` (broadcast, addressing with a reliable rolling code port
  `bc_rel` and a broadcast port `bc_all`), `raw` (size sweep, sent size in the header) and `raw_small` (100 bytes,
  oversize warning). `entity_transport` on three stacks (ETRX and SESS log lines) and checked 12/200/600 byte streams
  on the reliable tops. `chaos: "true"` makes the board hop channel for 3 s and later 15 s (`espnow.set_channel`), a
  clean way to lose frames without touching the other board.
- `entest.hpp`: `entest::Stream` checked stream (seq, sent size, pattern; counts ok/bad/gap/dup/trunc/foreign, `STAT`
  line). A stream on the top of a stack that also carries `entity_transport` starts with 0x1E (version 1, unknown
  type), so entity_transport drops it silently. The UDP only streams `P2PF` and `P2PT` are there too.
- `udplink.yaml` + `ud32.example.yaml`: six udp components (unicast peer, subnet broadcast, multicast with
  `listen_address`, peer plus test host, `point_to_point: false`, `point_to_point: true` on the broadcast). Multicast
  needs `listen_address: <group>` on the udp component or nothing is received. `udpraw.py SECS HOST...` sends a
  datagram per second to ports 21121/21125/21126 of each board from the computer and counts what the boards send to it
  on 21124. The `tick` and `stream_interval` substitutions set the load.
- `pairrun.sh PREFIX E32V D1V [SECS] [RUN]`: flashes `<PREFIX>32<E32V>.yaml` on the ESP32 and `<PREFIX>66<D1V>.yaml`
  on the D1 mini (build letter p from `PR_TREE`, any other from `BASE_TREE`), captures both from a reset and prints a
  summary per board for the prefix.
- `urlink.yaml` (prefix `ur`): minimal unicast A/B (one raw link plus `on_receive`), PR against base.
- `rglink.yaml` (prefix `rg`): existing users (packet_transport over UDP multicast and ESP-NOW, udp.write,
  espnow send and broadcast actions and triggers); remote entities for both peer names so one image serves four
  pairings.
- `encheck.py LOG...`: per link counts, gaps and bad float values from ETRX lines, last STAT per stream, warning
  histogram. `validation_cases.py`: writes 41 ok_/bad_ configs into `out-valid/`; run `esphome config` on each.

Facts:
- ESP-NOW v2 frames above 250 bytes from an ESP32 (IDF 5.5) arrive on an ESP8266 cut to 250 bytes, not dropped.
- On the ESP8266 a peer link hears the peer's broadcasts (no destination reported), so with a chatty broadcaster it
  logs "10 packets in a row could not be understood".
- The espnow send pool fills under a few hundred packets a minute; each refused send logs `[E] Failed to allocate
  send packet from pool`.
- On a busy Wi-Fi network about half of the broadcasts between a D1 mini and an ESP32 can be lost, the D1 logs
  `sendto() error 5` (lwIP ERR_IF) under load, and reliable UDP links on the broadcast address flap on dev too;
  unicast is about 95 to 100 %. Keep UDP loads light (counter tick 5 s, streams every 2 s) or the test measures the
  Wi-Fi. With Wi-Fi joined, `espnow.send` from the D1 to the ESP32 fails almost always (dev too).
- The subnet broadcast address guess assumes .255; on a wider subnet (a /21, say) check it really ends in .255.
