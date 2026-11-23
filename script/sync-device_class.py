#!/usr/bin/env python3

import importlib
from pathlib import Path
import re
import sys

# pylint: disable=import-error
from homeassistant.components.binary_sensor import BinarySensorDeviceClass
from homeassistant.components.button import ButtonDeviceClass
from homeassistant.components.cover import CoverDeviceClass
from homeassistant.components.event import EventDeviceClass
from homeassistant.components.number import NumberDeviceClass
from homeassistant.components.sensor import SensorDeviceClass
from homeassistant.components.switch import SwitchDeviceClass
from homeassistant.components.update import UpdateDeviceClass
from homeassistant.components.valve import ValveDeviceClass

# pylint: enable=import-error

sys.path.insert(0, str(Path(__file__).parent.parent))

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
    "update": UpdateDeviceClass,
    "valve": ValveDeviceClass,
}


def sub(path, pattern, repl, expected):
    with Path(path).open(encoding="utf-8") as handle:
        content = handle.read()
    content, count = re.subn(pattern, repl, content, flags=re.MULTILINE)
    if count != expected:
        raise RuntimeError(
            f"{path}: expected {expected} DEVICE_CLASS_ blocks, found {count}"
        )
    with Path(path).open("w", encoding="utf-8") as handle:
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
    sub("esphome/const.py", '(DEVICE_CLASS_\\w+ = "\\w*"\r?\n)+', out, 1)

    for domain in sorted(allowed):
        # replace imports
        out = ""
        for item in sorted(allowed[domain]):
            out += f"    DEVICE_CLASS_{item.upper()},\n"

        # The import list and DEVICE_CLASSES; 2+ lines skips the single class in sensor presets
        sub(
            f"esphome/components/{domain}/__init__.py",
            "(    DEVICE_CLASS_\\w+,\r?\n){2,}",
            out,
            2,
        )

    # A bad rewrite can still be valid Python that prek accepts; importing the modules catches it
    for domain in DOMAINS:
        importlib.import_module(f"esphome.components.{domain}")


if __name__ == "__main__":
    main()
