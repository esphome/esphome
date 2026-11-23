"""Write a GATT server yaml: gen_peri.py OUT NAME VARIANT BOARD SERVICES CHARS_PER_SERVICE [--burst]

--burst (the bt_proxy_ab server): ff01 writes are echoed as ff02 notifications, ff03 notifies a counter every
200 ms, and a fe00 service sends notification bursts: a write [count_hi, count_lo, per_tick] to fe03 starts 2 byte
sequence notifications on fe01, per_tick every 15 ms, then one notification on fe02 (ble_server sends a subscribed
indication without the ack) whose bytes 1-2 are the server's own send failures and byte 3 is 1 when srvconf.hpp
counts them (refused at once and reported late). Logs on UART0.
"""

import pathlib
import sys

if len(sys.argv) not in (7, 8) or sys.argv[7:] not in ([], ["--burst"]):
    sys.exit(__doc__)
out, name, variant, board, ns, nc = (
    sys.argv[1],
    sys.argv[2],
    sys.argv[3],
    sys.argv[4],
    int(sys.argv[5]),
    int(sys.argv[6]),
)
burst = sys.argv[7:] == ["--burst"]
L = [
    f"# {name}: idle GATT server, {ns} services x {nc} readable characteristics, each with a 2901 description",
    "esphome:",
    f"  name: {name}",
]
if burst:  # srvconf.hpp counts the server's send failures: refused at once, and late (bad ESP_GATTS_CONF_EVT)
    L += [
        f"  includes: [{pathlib.Path(__file__).resolve().parent / 'srvconf.hpp'}]",
        "  platformio_options:",
        "    build_flags:",
        "      - -Wl,--wrap=esp_ble_gatts_register_callback",
        "      - -Wl,--wrap=esp_ble_gatts_send_indicate",
    ]
L += [
    "esp32:",
    f"  variant: {variant}",
    f"  board: {board}",
    "  framework:",
    "    type: esp-idf",
    "    sdkconfig_options:",
    "      CONFIG_BT_GATT_MAX_SR_PROFILES: '32'",
    "      CONFIG_BT_GATT_MAX_SR_ATTRIBUTES: '500'",
    "logger:",
    "  level: DEBUG",
]
if burst:
    L += [
        "  hardware_uart: UART0",
    ]
elif variant != "ESP32":
    L.append("  hardware_uart: USB_SERIAL_JTAG")
L += [
    "esp32_ble:",
    f"  name: {name}",
    "esp32_ble_server:",
    "  on_connect:",
    "    - logger.log: {format: 'SRV connected', level: INFO}",
    "  on_disconnect:",
    "    - logger.log: {format: 'SRV disconnected', level: INFO}",
    "  services:",
]
for s in range(ns):
    L.append(f"    - uuid: 5ec2{s:02x}00-0000-1000-8000-00805f9b34fb")
    if s == 0:
        L.append("      advertise: true")
    L.append("      characteristics:")
    if s == 0 and burst:
        L += [
            "        - id: echoc",
            "          uuid: 5ec2ff02-0000-1000-8000-00805f9b34fb",
            "          read: true",
            "          notify: true",
            "          value: [0]",
            "        - id: tickc",
            "          uuid: 5ec2ff03-0000-1000-8000-00805f9b34fb",
            "          read: true",
            "          notify: true",
            "          value: [0, 0, 0, 0]",
        ]
    if s == 0:
        L += [
            "        - uuid: 5ec2ff01-0000-1000-8000-00805f9b34fb",
            "          write: true",
            "          read: true",
            "          value: [0]",
            "          on_write:",
        ]
        if burst:  # echo the write as an ff02 notification
            L += [
                "            - ble_server.characteristic.set_value:",
                "                id: echoc",
                "                value: !lambda return x;",
                "            - ble_server.characteristic.notify:",
                "                id: echoc",
            ]
        else:
            L.append(
                "            - logger.log: {format: 'SRV first command written', level: INFO}"
            )
    for c in range(nc):
        L += [
            f"        - uuid: 5ec2{s:02x}{c + 1:02x}-0000-1000-8000-00805f9b34fb",
            "          read: true",
            f"          value: 's{s}c{c}-value'",
            f"          description: 'svc {s} chr {c}'",
        ]

BURST = """\
    - uuid: 5ec2fe00-0000-1000-8000-00805f9b34fb
      characteristics:
        - id: burstc
          uuid: 5ec2fe01-0000-1000-8000-00805f9b34fb
          notify: true
          value: [0, 0]
        - id: finalc
          uuid: 5ec2fe02-0000-1000-8000-00805f9b34fb
          indicate: true
          value: [0]
        - uuid: 5ec2fe03-0000-1000-8000-00805f9b34fb
          write: true
          value: [0]
          on_write:
            - lambda: |-
                // x: [count_hi, count_lo, per_tick]; default 1000 notifications, 3 per 15 ms tick (about 200/s)
                id(b_total) = x.size() >= 2 ? (x[0] << 8 | x[1]) : 1000;
                id(b_per) = x.size() >= 3 ? x[2] : 3;
                id(b_sent) = 0;
                srvconf::refused = 0;
                srvconf::bad = 0;
                srvconf::active = true;
                ESP_LOGI("srv", "SRV burst start total=%u per_tick=%u", (unsigned) id(b_total), (unsigned) id(b_per));
globals:
  - {id: b_total, type: uint32_t, initial_value: "0"}
  - {id: b_sent, type: uint32_t, initial_value: "0"}
  - {id: b_per, type: uint32_t, initial_value: "3"}
  - {id: tick_n, type: uint32_t, initial_value: "0"}
interval:
  - interval: 15ms
    then:
      - lambda: |-
          if (id(b_sent) >= id(b_total)) return;
          for (uint32_t i = 0; i < id(b_per) && id(b_sent) < id(b_total); i++) {
            // b_sent moves on after notify(), so a failure of the last one is still inside the count window
            uint16_t n = id(b_sent);
            const uint8_t v[2] = {(uint8_t) n, (uint8_t) (n >> 8)};
            id(burstc)->set_value(v, sizeof(v));
            id(burstc)->notify();
            id(b_sent)++;
          }
          if (id(b_sent) == id(b_total)) {
            // the final value carries the server's own send failures (refused at once, or reported late by the
            // stack), so they are not taken for proxy loss; a lower bound, late reports after this one are missed
            srvconf::active = false;
            const uint32_t fails = srvconf::refused + srvconf::bad;
            // byte 3: both shims ran (the burst's sends went through the send wrap), so the count is complete
            const uint8_t f[4] = {0xF1, (uint8_t) fails, (uint8_t) (fails >> 8), (uint8_t) srvconf::complete()};
            id(finalc)->set_value(f, sizeof(f));
            id(finalc)->notify();
            ESP_LOGI("srv", "SRV burst done sent=%u send_failed=%u", (unsigned) id(b_sent), (unsigned) fails);
          }
  - interval: 200ms
    then:
      - lambda: |-
          // paused during a burst, so every send failure counted then is a fe01 burst notification
          if (id(b_sent) < id(b_total)) return;
          uint32_t n = ++id(tick_n);
          const uint8_t v[4] = {(uint8_t) n, (uint8_t) (n >> 8), (uint8_t) (n >> 16), (uint8_t) (n >> 24)};
          id(tickc)->set_value(v, sizeof(v));
          id(tickc)->notify();
          if (n % 3000 == 0) ESP_LOGI("srv", "SRV tick %u", (unsigned) n);
"""
if burst:
    L += BURST.splitlines()
pathlib.Path(out).write_text("\n".join(L) + "\n", encoding="utf-8")
