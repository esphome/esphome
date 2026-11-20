"""probe.py TREE apply|revert: temporary instrumentation of the Arduino I2C bus; never commit it.

apply stores SCL, SDA and the recovery result right after `recover_()` in a global that `i2cpico.yaml` logs.
revert removes only the probe text, so other edits to the file are kept.
"""

import pathlib
import sys

tree, action = sys.argv[1], sys.argv[2]
rel = "esphome/components/i2c/i2c_bus_arduino.cpp"
EDITS = (
    (
        "namespace esphome::i2c {",
        "int i2c_recovery_probe = 0;  // TEMP PROBE\nnamespace esphome::i2c {",
    ),
    (
        "  this->recover_();\n",
        (
            "  this->recover_();\n"
            "  ::i2c_recovery_probe = 0x100 | digitalRead(this->scl_pin_) | (digitalRead(this->sda_pin_) << 1) |\n"
            "                         (this->recovery_result_ << 4);  // TEMP PROBE\n"
        ),
    ),
)
path = pathlib.Path(tree, rel)
src = path.read_text(encoding="utf-8")
for plain, probed in EDITS:
    old, new = (probed, plain) if action == "revert" else (plain, probed)
    if action != "revert" and probed in src:
        sys.exit(f"{rel} already has the probe")
    if old not in src:
        sys.exit(f"{rel} did not match {old.strip()!r}; nothing was changed")
    src = src.replace(old, new, 1)
path.write_text(src, encoding="utf-8")
