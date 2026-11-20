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

`scripts/mqtt/mac_suffix/run.sh X BUILD` compiles (from `TREE`) and flashes variant X (a defaults, b custom `topic_prefix`, c explicit topics,
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
`sdrun.sh V N` (`TREE`, `P32` and `P66` in the environment) compiles and flashes the ESP32 and D1 mini, restarts each N times, disables MQTT once and deep sleeps the ESP32 three
times (never the D1 mini: without the GPIO16 to RST wire it never wakes); `match.py OUT` counts a `bye` within 5 s of
each command. Dev: ESP32 0/10 restarts, D1 mini 1 to 3/10. Put the broker address (`broker_ip`) in the secrets.yaml next to
the included package, and quote `"${name}/status"` inside flow mappings.

Results: https://github.com/esphome/esphome/pull/21566#issuecomment-6381448061

Retest at c9cf3c2813 (shutdown message sent at QoS 1, teardown waits for the broker's ack): 10/10 restarts on both
boards and 3/3 deep sleeps, against 1/10, 3/10 and 0/3 on base. An earlier 100 ms hold gave 3/10, 6/10 and 2/3, so
judge a delivery fix by counts over 10 or more restarts, never one or two presses.
Results: https://github.com/esphome/esphome/pull/21566#issuecomment-6382076613

## MQTT discovery against a throwaway Home Assistant (#21956)

Run a Home Assistant dev install with its own config directory (`hass -c <scratch>/ha`) holding `http:
server_port: 8129`, `api:`, `config:`, `onboarding:` and MQTT debug logging, plus a local mosquitto on 1883. In
`scripts/mqtt/ha_discovery/`, `onboard.py DIR` creates a generated test user (the credentials stay in DIR) and a
token, `flow.py TOKENFILE` adds the MQTT entry (the `protocol` and `other_settings: {set_client_cert: false,
set_ca_cert: "off", transport: tcp}` fields are required), and `hadrive.py TOKENFILE NAME` reads the device's time,
date and datetime entities, calls `set_value` on each and reads again; it exits 1 on a missing entity, a failed
call or a state that does not read back the value that was set. The three share the REST calls in `ha_rest.py`. `dt-common.yaml` is the device side (classic ESP32, three
template datetime entities, `${broker}`).

- The HA dev branch keeps a YAML port change as a pending HTTP config and reverts it after 5 minutes. Before
  starting HA again, set `data.stable.server_port` to 8129 and `data.pending` to null in `.storage/http`.
- ESPHome MQTT unique ids are name based: clear the previous build's retained discovery (`mosquitto_pub -r -n` on
  each config topic) and expect the entity ids of the first build.

## mqtt.disable and broker drops on ESP-IDF (#22230)

`scripts/mqtt/disable/disable-common.yaml` disables MQTT for 30 s every 60 s (every third cycle starts with fast
toggles, a disable while connecting), publishes a tick every 2 s and logs `EV CONNECT`, `EV DISCONNECT` and
`EV MSG`. `run.sh LABEL outage|drop SECONDS` (env `TREE`, `PORT`, `SC`, `PY`, `BROKER`, `BROKER_PORT`) starts a
private mosquitto so it can stop and start it, flashes and captures:
- `outage` publishes to `md<LABEL>/in` every second and stops the broker inside a disabled window; `analyze.py`
  prints per window the device connects, inbound messages, broker connects, ticks that reached the broker and the
  time from enable to connect, and fails the run on a device connect or an inbound message inside a window.
- `drop` stops the broker 6 s after every enable for 15 s. This is the case that found a watchdog reset: calling
  `esp_mqtt_client_stop()` while the esp-mqtt task sleeps in WAIT_RECONNECT blocks the caller for up to
  reconnect_timeout/2 (5 s), 6 of 6 task watchdog resets on an S3 and a C6.

Both modes also fail when the device never connects or the broker was never stopped, and `outage` when no inbound
message arrived.

Use the host's current address for `BROKER`: a stale one gives `esp-tls select() timeout`. Before #22230 the client
reconnects on its own during disable (14 of 16 windows). Results:
https://github.com/esphome/esphome/pull/22230#issuecomment-6614854671

## Retained message burst at connect (#22200)

`scripts/mqtt/retained_burst/`: `burst-topics.yaml` (a package) subscribes 25 `on_message` topics plus 15 switch
command topics, `burst-common.yaml` logs the counts 15 s after the connect, and `seed.sh BROKER_PORT` leaves all 40 retained on a local mosquitto (`seed.sh BROKER_PORT clear` removes them).
`run.sh LABEL BOOTS` (env `TREE`, `PORT`, `SC`, `PY`, `BROKER`, `BROKER_PORT`, default board an ESP32-C3) resets the
board BOOTS times and prints `RESULT got=x/25 switches_on=y/15` and any `Dropped` lines per boot; it passes only with
40 of 40 on every boot. Before #22200 the C3 dropped exactly 49 events (80 events minus 31 slots): the main loop
drains nothing during the burst.

## Forced reconnect mid-burst (#22248)

`scripts/mqtt/reconnect/`: `kick-common.yaml` includes the same `burst-topics.yaml` and counts per connection with
a `mode: restart` script on `on_connect`, logging `RESULT` 6 s after each connect. It and `run.sh` use files from
`../retained_burst/`, so copy the whole `mqtt/` directory to the scratch directory. `kick.py BROKER_LOG BROKER_PORT
PREFIX CYCLES` tails the broker log and takes over the device's client id 0 to 400 ms after every other connect
(mosquitto drops it with "session taken over"), then leaves the next connection alone for 8 s and ends it. It
starts by kicking the connection that is already up and skips its own connections (from 127.0.0.1 with the same id).
`run.sh LABEL CYCLES` (env as above plus `BROKER_LOG`) seeds, flashes, waits for the first connect and runs the
kicker. The device counts its first connection as CONN 1, so the kicked ones are the even CONN numbers and the
left-alone ones the odd numbers from 3 up; kick.py prints each left-alone connection with its time and CONN number,
and `judge.py` pairs those with the RESULT lines by time and passes when every one has `got=25/25`. A reset after
the first connect fails the run. Retained switch commands fire `on_turn_on` only on the first connection, so
`switches_on` is not judged.
