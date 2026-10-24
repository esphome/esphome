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
certificates), checks every listener in the broker config answers again, and prints the broker's message counts with the device log. `mosquitto_sub` cannot speak
WebSocket, so it subscribes on the plain listener of the same broker. Cases: `tcp` on 1883, `ws` on 9001, `wss` on
9002 with `certificate_authority` (the certificate names an address, so `skip_cert_cn_check: true`), `wss` to
test.mosquitto.org on 8081 with no CA for the certificate bundle, and two negatives that must never connect: a
different CA (`mbedtls_ssl_handshake returned -0x2700`) and the self-signed server through the bundle
(`esp-x509-crt-bundle: Failed to verify certificate`, `-0x3000`). `CONFIG_MQTT_TRANSPORT_WEBSOCKET` is on by
default in the generated sdkconfig. The TLS session costs about 29 kB of internal heap while connected; the public
broker carries other devices' `esphome/discover` traffic, which is noise. Results:
https://github.com/esphome/esphome/pull/16056#issuecomment-6301034019

## TLS trust sources on ESP-IDF (#14318)

One build per trust source from `templates/mqtt-tls-dut.yaml`: broker only (port must resolve to 1883),
`certificate_authority` with no port (must resolve to 8883), `verify_ssl: false` with no CA, the CA plus
`client_certificate` and `client_certificate_key` on the 8884 listener that requires one, `transport: wss` with the CA,
`tls: true` to test.mosquitto.org on 8886 for the certificate bundle, and `tls: true` to the local self-signed broker as
the negative. `scripts/mqtt/mosquitto-ws.conf` now has the 8883 and 8884 listeners and `scripts/mqtt/make_test_ca.sh`
also writes `client.crt`/`client.key`. Before flashing, check each build's `src/esphome/core/defines.h` for
`USE_MQTT_TLS`, `USE_MQTT_CERT_BUNDLE` and `USE_MQTT_TLS_INSECURE` and its `sdkconfig.<name>` for
`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE` and `CONFIG_ESP_TLS_INSECURE`: only the bundle builds may carry the bundle and only
the unverified build the insecure flags. `scripts/mqtt/flash_and_drive.sh TREE YAML PORT KEY BROKER_HOST
local|public|neg` runs one build end to end (flash, bounded capture, `cases.sh` with a restart for the local broker,
counts and the broker's per-listener connection lines) and prints the config dump's `TLS:` line (`CA certificate`,
`certificate bundle` or `no verification`). The CLI side (`esphome/mqtt.py`) is covered without a board by
`scripts/mqtt/cli_tls_check.sh TREE [neg] YAML...`, which runs `esphome clean-mqtt` under a timeout: status 124
after `Connected to MQTT broker!` is a pass, and with `neg` each config must instead fail with
`CERTIFICATE_VERIFY_FAILED` and never connect. Python's default SSL
context rejects a CA that lacks a `keyUsage` extension (`CA cert does not include key usage extension`) while mbedTLS
on the device accepts it, which is why the test CA carries `basicConstraints` and `keyUsage`. A broker kill shows as one
`Last esp-tls error: 0x8008` on every build, TLS or not. Results:
https://github.com/esphome/esphome/pull/14318#issuecomment-6311863827

## MAC suffix topics (#9315)

`scripts/mqtt/mac_suffix/run.sh X BUILD` flashes variant X (a defaults, b custom `topic_prefix`, c explicit topics,
d no suffix) of BUILD (`pr` or `base`) on an ESP32 and a D1 mini, records the broker with
`mosquitto_sub -F '@H:@M:@S r=%r %t %p'`, presses Restart through the `cmd_t` found in the discovery payloads, then
holds both chips in the bootloader for 40 s (`esptool --after no-reset chip-id`) so the broker sends the will; a
plain reset only gives "session taken over" and no will. The two boards' configs live in separate folders because
variants a to c share a `name` (the MAC suffix tells them apart), and the same name in one folder shares a build
directory. A D1 mini CH340 can garble the serial capture after an RTS reset, so read its topics from the broker.
`esphome logs --device MQTT` on a current tree needs paho-mqtt 2.x; an older venv with 1.6.1 fails, so use a venv
made from the tree's requirements.txt. A Restart press never delivered the shutdown message on any build tested.

## Shutdown message delivery (#21566)

`scripts/mqtt/shutdown/`: `sd-common.yaml` sets birth/will/shutdown payloads `hello`/`dead`/`bye` so the shutdown
message and the will cannot be confused, and takes `restart`, `disable` and `sleep` on `<name>/cmd`.
`sdrun.sh V N` (`TREE`, `P32` and `P66` in the environment) flashes the ESP32 and D1 mini, restarts each N times, disables MQTT once and deep sleeps the ESP32 three
times (never the D1 mini: without the GPIO16 to RST wire it never wakes); `match.py OUT` counts a `bye` within 5 s of
each command. Dev: ESP32 0/10 restarts, D1 mini 1 to 3/10. Put the broker address (`broker_ip`) in the secrets.yaml next to
the included package, and quote `"${name}/status"` inside flow mappings.

Results: https://github.com/esphome/esphome/pull/21566#issuecomment-6381448061

Retest at c9cf3c2813 (shutdown message sent at QoS 1, teardown waits for the broker's ack): 10/10 restarts on both
boards and 3/3 deep sleeps, against 1/10, 3/10 and 0/3 on base. An earlier 100 ms hold gave 3/10, 6/10 and 2/3, so
judge a delivery fix by counts over 10 or more restarts, never one or two presses.
Results: https://github.com/esphome/esphome/pull/21566#issuecomment-6382076613
