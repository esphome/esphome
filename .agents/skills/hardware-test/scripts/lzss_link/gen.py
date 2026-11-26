"""Write lz<board><variant>.yaml for the packet_interface compression test (#22471).

usage: gen.py BOARD VARIANT PEER_IP PEER_MAC
  BOARD    s3 (ESP32-S3, USB Serial/JTAG logger) or hz (Feather HUZZAH ESP8266)
  VARIANT  a  UDP, default dictionary
           b  UDP link capped at 200 bytes, compression max_packet_size 600 (oversize messages)
           c  mismatched dictionaries (each board a different one), VERBOSE log
           e  ESP-NOW, oversize messages
           n  control build without compression, for the base tree
  PEER_IP  the other board's address (UDP variants), PEER_MAC its MAC (ESP-NOW), as AA:BB:CC:DD:EE:FF
The dictionaries come from DICT_*.txt (the trailing newline is dropped).
"""

import json
from pathlib import Path
import sys

B, V, PEER_IP, PEER_MAC = sys.argv[1:5]
HERE = Path(__file__).parent
D = {
    i: (HERE / f"{n}.txt").read_text().rstrip("\n")
    for i, n in enumerate(["DICT_DEFAULT", "DICT_TYPICAL", "DICT_OTHER"])
}
node = 1 if B == "s3" else 2
dsel = {"n": 0, "a": 0, "b": 1, "e": 1, "c": 1 if B == "s3" else 2}[V]
oversize = V in "be"
verbose = V == "c"
name = f"lz{B}{V}"
begin = (
    f"lztest::begin_plain(id(lnk), {node});"
    if V == "n"
    else f"lztest::begin(id(lnk), id(cmp), {node}, {'true' if oversize else 'false'});"
)
header = (
    f"esphome:\n  name: {name}\n  includes: [lztest.hpp]\n  on_boot:\n    - priority: -100\n"
    f"      then:\n        - lambda: {begin}\n"
)
y = [header]
level = "VERBOSE" if verbose else "DEBUG"
if B == "s3":
    y.append("esp32:\n  board: esp32-s3-devkitc-1\n  framework:\n    type: esp-idf\n")
    y.append(f"logger:\n  hardware_uart: USB_SERIAL_JTAG\n  level: {level}\n")
else:
    y.append("esp8266:\n  board: huzzah\n")
    y.append(f"logger:\n  baud_rate: 115200\n  level: {level}\n")
if verbose:
    y.append(
        "  logs:\n    wifi: DEBUG\n    scheduler: DEBUG\n    component: DEBUG\n    udp: DEBUG\n    lztest: DEBUG\n"
    )
y.append("safe_mode:\n  disabled: true\n")
if V == "e":
    y.append("espnow:\n  channel: 1\n")
    link = f"  - platform: espnow\n    id: lnk\n    peer_address: {PEER_MAC}\n"
else:
    y.append(
        "wifi:\n  ssid: !secret wifi_ssid\n  password: !secret wifi_password\n  power_save_mode: none\n"
    )
    y.append(f'udp:\n  - id: u1\n    port: 21471\n    addresses: ["{PEER_IP}"]\n')
    link = "  - platform: udp\n    id: lnk\n    udp_id: u1\n    point_to_point: false\n"
    if V == "b":
        link += "    max_packet_size: 200\n"
    link += f"    addressing:\n      address: 0x000{node}\n      destination: 0x000{3 - node}\n"
link += "    reliability:\n"
if V != "n":
    link += "    compression:\n      id: cmp\n"
    link += f"      dictionary: {json.dumps(D[dsel])}\n"
if oversize:
    link += "      max_packet_size: 600\n"
link += "    session:\n"
y.append("packet_interface:\n" + link)
y.append(
    "interval:\n  - interval: 1s\n    then:\n      - lambda: lztest::tick();\n"
    "  - interval: 15s\n    then:\n      - lambda: lztest::stat();\n"
)
(HERE / f"{name}.yaml").write_text("".join(y))
print(f"{name}.yaml")
