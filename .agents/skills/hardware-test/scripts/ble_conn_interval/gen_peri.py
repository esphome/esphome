"""Write a GATT server yaml: gen_peri.py OUT NAME VARIANT BOARD SERVICES CHARS_PER_SERVICE"""

import pathlib
import sys

out, name, variant, board, ns, nc = (
    sys.argv[1],
    sys.argv[2],
    sys.argv[3],
    sys.argv[4],
    int(sys.argv[5]),
    int(sys.argv[6]),
)
L = [
    f"# {name}: idle GATT server, {ns} services x {nc} readable characteristics, each with a 2901 description",
    "esphome:",
    f"  name: {name}",
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
if variant != "ESP32":
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
    if s == 0:
        L += [
            "        - uuid: 5ec2ff01-0000-1000-8000-00805f9b34fb",
            "          write: true",
            "          read: true",
            "          value: [0]",
            "          on_write:",
            "            - logger.log: {format: 'SRV first command written', level: INFO}",
        ]
    for c in range(nc):
        L += [
            f"        - uuid: 5ec2{s:02x}{c + 1:02x}-0000-1000-8000-00805f9b34fb",
            "          read: true",
            f"          value: 's{s}c{c}-value'",
            f"          description: 'svc {s} chr {c}'",
        ]
pathlib.Path(out).write_text("\n".join(L) + "\n", encoding="utf-8")
