# mqtt

## MQTT (#20433, #17935, #20762, #20767, #20768)

A local mosquitto with `allow_anonymous true`, `log_type all` and `log_dest file`: the broker log gives
each subscription's QoS, the device address (`New client connected from <ip> as <name>-<mac>`) and exact
subscribe counts. Kill and restart mosquitto to exercise `on_disconnect` (reason 0) and resubscription.
Keep `reboot_timeout: 0s` on `api:` and `mqtt:`. The ESP-IDF outbox swings free heap about 2 kB right
after bulk subscribe and unsubscribe; read the heap seconds later over several rounds before calling a
leak. Spawned subscriptions need about 2.5 s to reach the broker before the next publish.
`esphome logs --device MQTT` only uses the MQTT transport when the config has no `api:`. Driver pattern: a
paho script publishes and polls the serial capture file.
