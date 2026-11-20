"""probe.py TREE apply|revert: temporary instrumentation of the Arduino I2C bus; never commit it.

apply stores SCL, SDA and the recovery result right after `recover_()` in a global that `i2cpico.yaml` logs.
"""

import pathlib
import subprocess
import sys

tree, action = sys.argv[1], sys.argv[2]
rel = "esphome/components/i2c/i2c_bus_arduino.cpp"
if action == "revert":
    subprocess.check_call(["git", "-C", tree, "checkout", "--", rel])
    sys.exit()
path = pathlib.Path(tree, rel)
src = path.read_text(encoding="utf-8")
src = src.replace(
    "namespace esphome::i2c {",
    "int i2c_recovery_probe = 0;  // TEMP PROBE\nnamespace esphome::i2c {",
    1,
)
src = src.replace(
    "  this->recover_();\n",
    "  this->recover_();\n"
    "  ::i2c_recovery_probe = 0x100 | digitalRead(this->scl_pin_) | (digitalRead(this->sda_pin_) << 1) |\n"
    "                         (this->recovery_result_ << 4);  // TEMP PROBE\n",
    1,
)
if "TEMP PROBE" not in src:
    sys.exit(f"{rel} did not match; the probe was not applied")
path.write_text(src, encoding="utf-8")
