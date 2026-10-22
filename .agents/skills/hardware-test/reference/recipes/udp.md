# udp

## UDP, multicast, sockets (#20556 to #20559, #20585, #20590)

A listener on port 9 needs no privileges on macOS. Test multicast device to device; host to device
multicast never arrives on some access points, and `239.255.x.x` groups are unreliable even to a stock
listener. Broadcast to boards is lossy under power save, unicast is not. Flood at a fixed packet rate
(the host hits `ENOBUFS` unthrottled); 1500 packets per second survived on every board. A forced Wi-Fi
reconnect on the Pico keeps IGMP membership. Set a long loop interval from `wifi.on_connect`
(`App.set_loop_interval(20000)`), not `on_boot`, or Wi-Fi connect crawls. The `logger.log` action
rejects `%.*s`; use a lambda.
