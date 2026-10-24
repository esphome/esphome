# Proven test flows

Each flow names the pull requests it was used on, so their result tables show what to post.

One file per component, named after it:

- [`api.md`](api.md): noise handshake and OTA encryption on the ESP8266, outgoing connection wake timing on deep sleep and after a Wi-Fi drop, user defined action listing and execution with a raw plaintext client, store_yaml recovery over Noise or plaintext
- [`ble_midi.md`](ble_midi.md): BLE MIDI client against a fake ESP32 instrument, every packet shape and the send actions
- [`captive_portal.md`](captive_portal.md): fallback AP and /wifisave hammer
- [`cse7766.md`](cse7766.md): Sonoff S31 parser check over the API
- [`espnow.md`](espnow.md): ESP8266 broadcast receive
- [`hdmi_cec.md`](hdmi_cec.md): two-board wire
- [`http_request.md`](http_request.md): update manifest URL changed at run time, every setter shape, a swap under a running check, non-blocking request actions with loop gap, queue, cancel and reboot checks
- [`ld2450.md`](ld2450.md): polygon zones, zone number round trip through the radar's readback, ESP8266 loopback harness without a radar
- [`ld2412.md`](ld2412.md): output pin level through the basic configuration command, held back writes
- [`libretiny.md`](libretiny.md): rtc preference emulation, BK72xx crash report on the next boot
- [`modem.md`](modem.md): PPPoS modem component boot check with no modem attached, loop blocking and disable timing
- [`mqtt.md`](mqtt.md): broker bench, QoS, reconnect, crash log over MQTT, WebSocket transports with a local and a public broker, TLS trust sources (own CA, client certificate, certificate bundle, no verification) with the CLI check, MAC suffix topics and will messages on two boards
- [`nrf52.md`](nrf52.md): dongle board support
- [`online_image.md`](online_image.md): stalled and malformed downloads, PNG, JPEG, BMP and QOI decode time per loop pass with large images and small buffers
- [`openthread.md`](openthread.md): border router on the S3 host plus H2 RCP
- [`ota.md`](ota.md): overlap and lock, timeouts, deflate
- [`rtsp_server.md`](rtsp_server.md): camera streaming on the Waveshare ESP32-S3-ETH
- [`scheduler.md`](scheduler.md): loop wake A/B
- [`udp.md`](udp.md): datagrams, multicast, sockets, `udp.write` payload shapes
- [`usb_cdc_acm.md`](usb_cdc_acm.md): nRF52840 dongle host driver
- [`wifi.md`](wifi.md): reboot_timeout with a fallback AP, DHCP lease cache, Easy Connect (DPP), WISP bridge with an ethernet upstream
- [`zephyr.md`](zephyr.md): preferences on the nRF52840 dongle
- [`zephyr_coredump.md`](zephyr_coredump.md): crash and decode on nRF52 boards
- [`zwave_proxy.md`](zwave_proxy.md): frame checksums on a live proxy
- [`globals.md`](globals.md): restoring globals with storage rtc and flash across every reset kind
- [`homeassistant.md`](homeassistant.md): light platform mirrored from a local Home Assistant, held reports versus later calls
- [`usb_audio.md`](usb_audio.md): USB audio speaker and microphone, the usb_host FIFO split, and Music Assistant playback through sendspin
- [`dfrobot_sen0395.md`](dfrobot_sen0395.md): SEN0395 mmWave command flow, module quirks and overlong lines through its error reply
- [`speaker.md`](speaker.md): media player pipeline tasks on the Connect AUX-2 with the `speaker` platform player
- [`web_server.md`](web_server.md): state SSE stress and the HTTPD task stack on web_server_idf, entity icon and device class readback
- [`uart_sim.md`](uart_sim.md): UART sensors simulated on a classic ESP32 with loopback and cross-wired UARTs
- [`gpio_expander.md`](gpio_expander.md): interrupt driven expander inputs, loop idle checks, testing a device out of reach
- [`led_strip.md`](led_strip.md): forcing a failed strip allocation, RMT symbols on the S3
