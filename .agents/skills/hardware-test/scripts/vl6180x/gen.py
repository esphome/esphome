"""gen.py NAME SCALER GAIN IT_MS PINS [EXTRA_FILE...]: print one VL6180X config for an ESP8266 NodeMCU.

SCALER 0 leaves out the distance sensor; PINS 1 adds enable_pin GPIO14 (XSHUT). Extra files are appended; the items
of every extra that starts with `interval:` are merged under one `interval:` key.
"""

import pathlib
import sys

name, scaler, gain, it, pins = sys.argv[1:6]
texts = [pathlib.Path(f).read_text(encoding="utf-8") for f in sys.argv[6:]]
items = [t.removeprefix("interval:\n") for t in texts if t.startswith("interval:\n")]
extra = "".join(t for t in texts if not t.startswith("interval:\n"))
if items:
    extra += "interval:\n" + "".join(items)
pinblk = "    enable_pin: GPIO14\n" if pins == "1" else ""
rng = (
    f"    distance:\n      name: Distance\n      scaler: {scaler}\n"
    if scaler != "0"
    else ""
)
print(f"""esphome:
  name: {name}
esp8266:
  board: nodemcuv2
logger:
  level: VERBOSE
  logs:
    sensor: VERBOSE
    scheduler: DEBUG
    component: DEBUG
    i2c: DEBUG
i2c:
  id: bus_a
  sda: GPIO4
  scl: GPIO5
vl6180x:
  - id: tof
{pinblk}    update_interval: 1s
sensor:
  - platform: vl6180x
    vl6180x_id: tof
{rng}    illuminance:
      name: Illuminance
      gain: {gain}
      integration_time: {it}ms
{extra}""")
