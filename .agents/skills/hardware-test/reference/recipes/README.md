# Proven test flows

Each flow names the pull requests it was used on, so their result tables show what to post.

One file per component, named after it:

- [`api.md`](api.md): noise handshake and OTA encryption on the ESP8266, outgoing connection wake timing on deep sleep and after a Wi-Fi drop
- [`captive_portal.md`](captive_portal.md): fallback AP and /wifisave hammer
- [`cse7766.md`](cse7766.md): Sonoff S31 parser check over the API
- [`espnow.md`](espnow.md): ESP8266 broadcast receive
- [`hdmi_cec.md`](hdmi_cec.md): two-board wire
- [`ld2450.md`](ld2450.md): polygon zones, zone number round trip through the radar's readback, ESP8266 loopback harness without a radar
- [`libretiny.md`](libretiny.md): rtc preference emulation, BK72xx crash report on the next boot
- [`mqtt.md`](mqtt.md): broker bench, QoS, reconnect, crash log over MQTT, WebSocket transports with a local and a public broker, TLS trust sources (own CA, client certificate, certificate bundle, no verification) with the CLI check
- [`nrf52.md`](nrf52.md): dongle board support
- [`online_image.md`](online_image.md): stalled and malformed downloads
- [`openthread.md`](openthread.md): border router on the S3 host plus H2 RCP
- [`ota.md`](ota.md): overlap and lock, timeouts, deflate
- [`rtsp_server.md`](rtsp_server.md): camera streaming on the Waveshare ESP32-S3-ETH
- [`scheduler.md`](scheduler.md): loop wake A/B
- [`udp.md`](udp.md): datagrams, multicast, sockets, `udp.write` payload shapes
- [`usb_cdc_acm.md`](usb_cdc_acm.md): nRF52840 dongle host driver
- [`wifi.md`](wifi.md): reboot_timeout with a fallback AP, DHCP lease cache, Easy Connect (DPP)
- [`zephyr.md`](zephyr.md): preferences on the nRF52840 dongle
- [`zephyr_coredump.md`](zephyr_coredump.md): crash and decode on nRF52 boards
- [`zwave_proxy.md`](zwave_proxy.md): frame checksums on a live proxy
- [`globals.md`](globals.md): restoring globals with storage rtc and flash across every reset kind
- [`homeassistant.md`](homeassistant.md): light platform mirrored from a local Home Assistant, held reports versus later calls
- [`usb_audio.md`](usb_audio.md): USB audio speaker and microphone, the usb_host FIFO split, and Music Assistant playback through sendspin
- [`dfrobot_sen0395.md`](dfrobot_sen0395.md): SEN0395 mmWave command flow, module quirks and overlong lines through its error reply
- [`speaker.md`](speaker.md): media player pipeline tasks on the Connect AUX-2 with the `speaker` platform player
- [`web_server.md`](web_server.md): state SSE stress and the HTTPD task stack on web_server_idf, entity icon and device class readback
