"""gen_sensors.py: write putbe-sensors.yaml, the BS_COUNT (60, txt.hpp) template sensors bs0..bs59 that
putbe-common.yaml floods, and the boot lambda that fills the BS pointer array its flood lambda loops over."""

import pathlib

n = 60  # BS_COUNT in txt.hpp
ids = ", ".join(f"id(bs{i})" for i in range(n))
out = [
    "esphome:",
    "  on_boot:",
    "    priority: 600",
    "    then:",
    "      - lambda: |-",
    f"          sensor::Sensor *all[] = {{{ids}}};",
    "          for (size_t i = 0; i < BS_COUNT; i++) BS[i] = all[i];",
    "sensor:",
]
for i in range(n):
    out += [
        "  - platform: template",
        f"    id: bs{i}",
        f"    name: S{i}",
        "    update_interval: never",
        "    accuracy_decimals: 0",
    ]
pathlib.Path(__file__).with_name("putbe-sensors.yaml").write_text(
    "\n".join(out) + "\n", encoding="utf-8"
)
