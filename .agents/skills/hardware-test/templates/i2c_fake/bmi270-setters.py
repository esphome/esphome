"""setters.py TREE OUT NAME=main.cpp...: write 'name setter value' lines for the harness from generated main.cpp."""

import pathlib
import re
import sys

tree, out = sys.argv[1], sys.argv[2]
h = pathlib.Path(f"{tree}/esphome/components/bmi270/bmi270.h").read_text(
    encoding="utf-8"
)
enums = {
    m[0]: int(m[1], 0)
    for m in re.findall(r"(BMI270_[A-Z0-9_]+) = (0x[0-9A-Fa-f]+|\d+)", h)
}
with pathlib.Path(out).open("w", encoding="utf-8") as f:
    for arg in sys.argv[3:]:
        name, path = arg.split("=")
        calls = re.findall(
            r"imu->(set_(?:accel|gyro|aux)\w*)\((?:bmi270::)?(\w+)\);",
            pathlib.Path(path).read_text(encoding="utf-8"),
        )
        if not calls:
            f.write(f"{name} none 0\n")
        f.writelines(
            f"{name} {fn} {enums[v] if v in enums else int(v, 0)}\n" for fn, v in calls
        )
