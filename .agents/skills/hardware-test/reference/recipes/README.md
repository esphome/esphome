# Proven test flows

Each flow names the pull requests it was used on, so their result tables show what to post.

One file per component, named after it:

- [`api.md`](api.md): noise handshake and OTA encryption on the ESP8266, outgoing connection wake timing on deep sleep and after a Wi-Fi drop, user defined action listing and execution with a raw plaintext client, store_yaml recovery over Noise or plaintext
- [`ble_midi.md`](ble_midi.md): BLE MIDI client against a fake ESP32 instrument, every packet shape and the send actions
- [`captive_portal.md`](captive_portal.md): fallback AP and /wifisave hammer
- [`cse7766.md`](cse7766.md): Sonoff S31 parser check over the API
- [`espnow.md`](espnow.md): ESP8266 broadcast receive
- [`hdmi_cec.md`](hdmi_cec.md): two-board wire
- [`http_request.md`](http_request.md): update manifest URL changed at run time, every setter shape, a swap under a running check, non-blocking request actions with loop gap, queue, cancel and reboot checks, OTA request cases with basic auth and MD5 sources
- [`ld2450.md`](ld2450.md): polygon zones, zone number round trip through the radar's readback, ESP8266 loopback harness without a radar, presence hold and link loss on a frame stream, BT MAC readback
- [`ld2412.md`](ld2412.md): output pin level through the basic configuration command, held back writes
- [`libretiny.md`](libretiny.md): rtc preference emulation, BK72xx and RTL8720C crash report on the next boot with a ROM dump matcher, BK7238 BLE bond store safety with a full flash backup and per partition diff, BK7238 OTA versus the board layout and bootloader
- [`modem.md`](modem.md): PPPoS modem component boot check with no modem attached, loop blocking and disable timing
- [`mqtt.md`](mqtt.md): broker bench, QoS, reconnect, crash log over MQTT, WebSocket transports with a local and a public broker, TLS trust sources (own CA, client certificate, certificate bundle, no verification) with the CLI check, MAC suffix topics and will messages on two boards, shutdown message delivery on restart, disable and deep sleep
- [`nrf52.md`](nrf52.md): dongle board support, zephyr_ble_server connection slot under reordered callbacks, SMP over UDP OTA over Thread with reset faults
- [`online_image.md`](online_image.md): stalled and malformed downloads, PNG, JPEG, BMP and QOI decode time per loop pass with large images and small buffers
- [`openthread.md`](openthread.md): border router on the S3 host plus H2 RCP
- [`ota.md`](ota.md): upload without a configuration and the refusal checks, overlap and lock, timeouts, deflate, upload timing A/B per build and board with deflate and raw, on-device inflate benchmark, OTA automations on two platforms
- [`rtsp_server.md`](rtsp_server.md): camera streaming on the Waveshare ESP32-S3-ETH
- [`scheduler.md`](scheduler.md): loop wake A/B, interval startup delay and control from lambdas
- [`udp.md`](udp.md): datagrams, multicast, sockets, `udp.write` payload shapes
- [`usb_cdc_acm.md`](usb_cdc_acm.md): nRF52840 dongle host driver
- [`wifi.md`](wifi.md): reboot_timeout with a fallback AP, DHCP lease cache, Easy Connect (DPP), WISP bridge with an ethernet upstream, reboots against a WPA2/WPA3 PMF access point, WISP and ALWAYS_ON AP switching, AP restarts with WPA3 mixed and LIGHT power save
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
- [`led_strip.md`](led_strip.md): forcing a failed strip allocation, RMT symbols per chip, loop cost per strip
- [`remote_base.md`](remote_base.md): remote_receiver protocol decoding through a one-pin loopback on ESP32 and ESP8266, no IR hardware
- [`xiaomi_ble.md`](xiaomi_ble.md): Xiaomi, ATC and PVVX parsing by injecting scan results into the ESP32 tracker or advertising them from a second ESP32, real AES-CCM frames, duplicate filter model
- [`ble_terminate.md`](ble_terminate.md): proxy disconnect against a peer with peripheral latency
- [`i2c_slave.md`](i2c_slave.md): fake I2C slave on a D1 mini, ESP32 hardware master as the control
- [`gpio_hold.md`](gpio_hold.md): GPIO hold through the ISR pin API, pad read back with no wiring, per chip pin notes
- [`ble_event_inject.md`](ble_event_inject.md): made up GAP and GATTC events fed into the ESP32 BLE handlers from a button
- [`light_sleep_gpio_wake.md`](light_sleep_gpio_wake.md): GPIO wakeup from auto light sleep with an LEDC wave that keeps running in sleep, and with an external wave and peripheral power-down on the ESP32-P4
- [`manual_ip_and_web_handlers.md`](manual_ip_and_web_handlers.md): station, AP and Ethernet manual IP, web handlers added late and web OTA with web_server, captive_portal or prometheus
- [`ble_server_security.md`](ble_server_security.md): esp32_ble_server pairing, encrypted and MITM characteristics, subscription before and after pairing, bond reuse per ESP-IDF version
- [`prometheus.md`](prometheus.md): date, time and datetime metrics in UTC and another time zone
- [`ble_adv_triggers.md`](ble_adv_triggers.md): BLE service and manufacturer data advertise triggers over the air on the ESP32 and BK72xx trackers, with a second ESP32 advertising known frames
- [`sun.md`](sun.md): on_sunrise and on_sunset by sweeping the observer's longitude, one full sweep in about 16 minutes
- [`sensor_filters.md`](sensor_filters.md): every sensor, binary_sensor and text_sensor filter shape fed a fixed input sequence, outputs compared bit for bit between builds
- [`led_driver_sniff.md`](led_driver_sniff.md): bit-banged LED drivers captured on the ESP32's own pads with SPI slaves through the GPIO matrix, no chips
- [`hub_input_sim.md`](hub_input_sim.md): matrix_keypad, wiegand, rdm6300 and key_collector driven from a D1 mini's own pins and UART loopback
- [`value_range.md`](value_range.md): sensor and number on_value_range with a fixed sequence and a restart
- [`ezo_fake_bus.md`](ezo_fake_bus.md): the ezo component against a fake I2C bus that answers like an EZO circuit
- [`twai_self.md`](twai_self.md): esp32_can on_frame with the controller hearing its own transmit pad, no transceiver
- [`buttons.md`](buttons.md): output button pad timing, homeassistant button through a raw API client with the wizard input, uart button on the UART0 loopback
