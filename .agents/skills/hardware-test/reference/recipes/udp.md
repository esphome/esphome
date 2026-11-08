# udp

## UDP, multicast, sockets (#20556 to #20559, #20585, #20590)

A listener on port 9 needs no privileges on macOS. Test multicast device to device; host to device
multicast never arrives on some access points, and `239.255.x.x` groups are unreliable even to a stock
listener. Broadcast to boards is lossy under power save, unicast is not. Flood at a fixed packet rate
(the host hits `ENOBUFS` unthrottled); 1500 packets per second survived on every board. A forced Wi-Fi
reconnect on the Pico keeps IGMP membership. Set a long loop interval from `wifi.on_connect`
(`App.set_loop_interval(20000)`), not `on_boot`, or Wi-Fi connect crawls. The `logger.log` action
rejects `%.*s`; use a lambda.

## udp.write payload shapes (#21263, ApplyBytes)

`templates/udp-write-dut.yaml` puts every payload shape behind a template button (constant 4 bytes twice, 3, 9, 0,
128, 129 and 600 bytes, a 300 byte lambda, a simple value, a string) plus an `on_receive` that replies with the
received bytes reversed. `scripts/udp/write_cases.sh DEVICE_IP KEY` records the device's datagrams with
`udp_listen.py` on the test host, presses the buttons two seconds apart, sends one unicast datagram for the echo and
compares everything byte for byte with `check.py`; a missing datagram shifts every later one, so read the first
mismatch. Before flashing, check the generated `main.cpp`: on ESP8266 a constant payload is copied out of its
`PROGMEM` table into `uint8_t esphome__bytes[N]` up to the stack limit and into `std::make_unique<uint8_t[]>` beyond,
identical payloads share one table, a lambda is sent from its vector. An empty constant payload is generated as
`send_packet(nullptr, 0)`, and the raw lwIP UDP `sendto` used on ESP8266 rejected a null buffer with `EINVAL`
(`sendto() error 22` in the log, no datagram) until it accepted `nullptr` with a zero length; the old
`TemplatableBytes::visit` had passed a zero length stack buffer instead. Compare sizes against the stacked PR's own
head when the PR is stacked, not against dev. Results: https://github.com/esphome/esphome/pull/21263
