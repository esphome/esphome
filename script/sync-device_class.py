#!/usr/bin/env python3

import re

# pylint: disable=import-error
from homeassistant.components.binary_sensor import BinarySensorDeviceClass
from homeassistant.components.button import ButtonDeviceClass
from homeassistant.components.cover import CoverDeviceClass
from homeassistant.components.event import EventDeviceClass
from homeassistant.components.number import NumberDeviceClass
from homeassistant.components.sensor import SensorDeviceClass
from homeassistant.components.switch import SwitchDeviceClass
from homeassistant.components.valve import ValveDeviceClass

# pylint: enable=import-error

BLOCKLIST = (
    # requires special support on HA side
    "enum",
)

DOMAINS = {
    "binary_sensor": BinarySensorDeviceClass,
    "button": ButtonDeviceClass,
    "cover": CoverDeviceClass,
    "event": EventDeviceClass,
    "number": NumberDeviceClass,
    "sensor": SensorDeviceClass,
    "switch": SwitchDeviceClass,
    "valve": ValveDeviceClass,
}


def sub(path, pattern, repl):
    with open(path, encoding="utf-8") as handle:
        content = handle.read()
    content = re.sub(pattern, repl, content, flags=re.MULTILINE)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(content)


def main():
    classes = {"EMPTY": ""}
    allowed = {}

    for domain, enum in DOMAINS.items():
        available = {
            cls.value.upper(): cls.value for cls in enum if cls.value not in BLOCKLIST
        }

        classes.update(available)
        allowed[domain] = list(available.keys()) + ["EMPTY"]

    # replace constant defines in const.py
    out = ""
    for cls in sorted(classes):
        out += f'DEVICE_CLASS_{cls.upper()} = "{classes[cls]}"\n'
    sub("esphome/const.py", '(DEVICE_CLASS_\\w+ = "\\w*"\r?\n)+', out)

    for domain in sorted(allowed):
        # replace imports
        out = ""
        for item in sorted(allowed[domain]):
            out += f"    DEVICE_CLASS_{item.upper()},\n"

        sub(
            f"esphome/components/{domain}/__init__.py",
            "(    DEVICE_CLASS_\\w+,\r?\n)+",
            out,
        )


if __name__ == "__main__":
    main()
