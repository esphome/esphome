# wled

## Realtime sender on the bench (#22509)

When the access point stops passing traffic between clients (lwIP `sendto` on the sender fails with errno 12
because ARP never resolves), run the sender on a board instead of the host: an ESP32-S3 runs `wifi: ap:` and a
25 ms interval lambda sends WARLS, DRGB or DNRGB frames with lwIP `sendto` to the devices under test, which join
that AP with `manual_ip`. Two lights on the same port: see `udp.md`. Heap after an effect stops: see
`heap_alloc.md`.
