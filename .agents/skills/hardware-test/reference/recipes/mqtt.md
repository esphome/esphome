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

## WebSocket transports on ESP-IDF (#16056)

One build per transport from `templates/mqtt-transport-dut.yaml`, with `scripts/mqtt/mosquitto-ws.conf` as the
local broker (mosquitto 2.x from brew has `websockets` support; `log_type all` prints `New connection from <ip>:<port>
on port <listener>`, which proves which listener each client used) and `scripts/mqtt/make_test_ca.sh BROKER_IP` for
the TLS listener's certificate, both run in the scratch directory so keys, logs and outputs stay out of the checkout. `scripts/mqtt/cases.sh NAME KEY BROKER_HOST SUB_PORT CAPTURE [DEV_HOST] [restart]` subscribes
to `NAME/#`, sends a switch command and an `on_message` topic, presses the 3000 byte publish button over the API,
restarts the local broker when the last argument is `restart` (the PID in `mosquitto.pid`, written when the broker
was started with `mosquitto -c mosquitto-ws.conf & echo $! > mosquitto.pid` in the working directory next to the
certificates), checks its three ports answer again, and prints the broker's message counts with the device log. `mosquitto_sub` cannot speak
WebSocket, so it subscribes on the plain listener of the same broker. Cases: `tcp` on 1883, `ws` on 9001, `wss` on
9002 with `certificate_authority` (the certificate names an address, so `skip_cert_cn_check: true`), `wss` to
test.mosquitto.org on 8081 with no CA for the certificate bundle, and two negatives that must never connect: a
different CA (`mbedtls_ssl_handshake returned -0x2700`) and the self-signed server through the bundle
(`esp-x509-crt-bundle: Failed to verify certificate`, `-0x3000`). `CONFIG_MQTT_TRANSPORT_WEBSOCKET` is on by
default in the generated sdkconfig. The TLS session costs about 29 kB of internal heap while connected; the public
broker carries other devices' `esphome/discover` traffic, which is noise. Results:
https://github.com/esphome/esphome/pull/16056#issuecomment-6301034019
