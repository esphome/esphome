"""Write the ESP32-C6 configs for the duco box model: gen.py NAME pr|dev."""

import os
import pathlib
import sys

name, kind = sys.argv[1], sys.argv[2]
# DUCO_VERBOSE=1 shows the hub's discovery lines (Found node ...) and its sent requests
SERIAL_SIZE = int(os.environ.get("DUCO_SERIAL_SIZE", "34"))
LEVEL = "VERBOSE" if os.environ.get("DUCO_VERBOSE") else "DEBUG"

# (key, id, address, expected) in the PR schema; expected None means no state may be published
PR_TEXT = [
    ("serial_number", "serial_number_node_1", 1, "SN2134"),
    ("serial_number", "serial_number_node_2", 2, "SN2234"),
    ("serial_number", "invalid_serial_number", 3, None),
    ("version", "version", 4, "1.2.3.4"),
    (
        "error",
        "node_error",
        3,
        "E31.19.01  Filters need replacement.\\nE99.99.99  Unknown DUCO error\\n",
    ),
    ("error", "node_no_error", 2, "No errors"),
    ("error", "node_zero_errors", 4, "No errors"),
    ("error", "node_short_errors", 5, None),
    (
        "error",
        "node_many_errors",
        6,
        "E03.01.04\\nE04.01.04\\nE06.01.04\\nE16.01.04\\n",
    ),
    (
        "error",
        "node_three_long_errors",
        7,
        "E16.01.04  Extract air pressure sensor value is unchanged for too long.\\nE29.01.04  Supply air pressure sensor value is unchanged for too long.\\nE30.01.04  SUP temperature sensor value is unchanged for too long.\\n",
    ),
    (
        "error",
        "node_four_short_errors",
        8,
        "E31.19.01  Filters need replacement.\\n" * 4,
    ),
    ("uptime", "uptime", 2, "1d 12h 30m"),
    ("node_type", "node_type", 2, "BSRH"),
    ("node_type", "node_type_node_1", 1, "BOX"),
    ("node_type", "node_type_node_3", 3, "UCCO2"),
]
SENSORS = [
    ("co2", 3),
    ("temperature", 3),
    ("humidity", 3),
    ("filter_remaining", None),
    ("flow_level", None),
    ("time_remaining", None),
    ("bypass", None),
    ("temperature_oda", None),
    ("temperature_sup", None),
    ("temperature_eta", None),
    ("temperature_eha", None),
]

ids = [s for s, _ in SENSORS] + ["serial_number"]
if kind == "pr":
    ids += [t[1] for t in PR_TEXT]
ids.append("comfort_temperature")

out = []
w = out.append
w(f"""esphome:
  name: {name}
  includes:
    - ducosim.hpp
  on_boot:
    priority: -100
    then:
      - lambda: ducosim::bus = id(box_uart); ducosim::box_serial_size = {SERIAL_SIZE};
      - delay: 1s
      - logger.log: "BURST 1"
      - script.execute: burst
      - delay: 8s
      - logger.log: "SET mode CNT2"
      - select.set:
          id: mode
          option: CNT2
      - delay: 3s
      - logger.log: "SET comfort 22.3"
      - number.set:
          id: comfort_temperature
          value: 22.3
      - delay: 3s
      - logger.log: "PHASE1 DONE"

esp32:
  variant: esp32c6
  board: esp32-c6-devkitc-1
  framework:
    type: esp-idf

logger:
  hardware_uart: USB_SERIAL_JTAG
  level: {LEVEL}
  logs:
    box: DEBUG
    text_sensor: DEBUG
    sensor: DEBUG
    select: DEBUG
    number: DEBUG
    preferences: DEBUG
    wifi: INFO
    wifi_esp32: INFO
    component: DEBUG
    scheduler: DEBUG

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

time:
  - platform: sntp
    id: sntp_time

# duco hub on one UART, the box model on the other, cross-wired through the GPIO matrix
uart:
  - id: duco_uart
    tx_pin:
      number: GPIO18
      allow_other_uses: true
    rx_pin:
      number: GPIO19
      allow_other_uses: true
    baud_rate: 57600
  - id: box_uart
    tx_pin:
      number: GPIO19
      allow_other_uses: true
    rx_pin:
      number: GPIO18
      allow_other_uses: true
    baud_rate: 57600

interval:
  - interval: 3ms
    then:
      - lambda: ducosim::poll();
  - interval: 20s
    then:
      - logger.log: "BURST again"
      - script.execute: burst
  - interval: 30s
    then:
      - lambda: 'ESP_LOGI("hwt", "ALIVE requests %u free heap %u", (unsigned) ducosim::requests, (unsigned) esp_get_free_heap_size());'

script:
  - id: burst
    then:
""")
for i in ids:
    w(f"      - component.update: {i}\n")
w("""
duco:
  uart_id: duco_uart
  discovery: {}
  time:
    time_id: sntp_time
    update_interval: 10s

select:
  - platform: duco
    id: mode
    name: Mode
    update_interval: 1s
    on_value:
      - lambda: 'ESP_LOGI("hwt", "STATE mode = %s", x.c_str());'

number:
  - platform: duco
    id: comfort_temperature
    name: Comfort Temperature
    update_interval: never
    on_value:
      - lambda: 'ESP_LOGI("hwt", "STATE comfort_temperature = %.2f", x);'

text_sensor:
  - platform: duco
    id: serial_number
    name: Serial Number
    update_interval: never
    on_value:
      - lambda: 'ducosim::show("serial_number", x);'
""")
if kind == "pr":
    w("  - platform: duco\n")
    for key in dict.fromkeys(t[0] for t in PR_TEXT):
        w(f"    {key}:\n")
        for k, i, addr, _ in PR_TEXT:
            if k != key:
                continue
            w(f"""      - address: {addr}
        id: {i}
        name: {i.replace("_", " ").title()}
        update_interval: never
        on_value:
          - lambda: 'ducosim::show("{i}", x);'
""")
w("\nsensor:\n  - platform: duco\n")
for s, addr in SENSORS:
    w(f"    {s}:\n")
    pre = "      "
    if addr is not None:
        w(f"      - address: {addr}\n")
        pre = "        "
    w(f"""{pre if addr is not None else pre}id: {s}
{pre}name: {s.replace("_", " ").title()}
{pre}update_interval: never
{pre}on_value:
{pre}  - lambda: 'ESP_LOGI("hwt", "STATE {s} = %.2f", x);'
""")
pathlib.Path(f"{name}.yaml").write_text("".join(out), encoding="utf-8")
if kind == "pr":
    with pathlib.Path("expected.txt").open("w", encoding="utf-8") as f:
        for _, i, _, e in PR_TEXT:
            f.write(f"{i}\t{e}\n")
