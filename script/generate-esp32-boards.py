#!/usr/bin/env python3

import json
import os
import subprocess
import tempfile

from esphome.components.esp32 import ESP_IDF_PLATFORM_VERSION as ver

version_str = f"{ver.major}.{ver.minor:02d}.{ver.patch:02d}"
print(f"ESP32 Platform Version: {version_str}")


def get_boards():
    with tempfile.TemporaryDirectory() as tempdir:
        subprocess.run(
            [
                "git",
                "clone",
                "--depth",
                "1",
                "--branch",
                f"{ver.major}.{ver.minor:02d}.{ver.patch:02d}",
                "https://github.com/pioarduino/platform-espressif32",
                tempdir,
            ],
            check=True,
        )
        boards_file = os.path.join(tempdir, "boards")
        boards = {}
        for fname in os.listdir(boards_file):
            if not fname.endswith(".json"):
                continue
            with open(os.path.join(boards_file, fname), encoding="utf-8") as f:
                board_info = json.load(f)
                mcu = board_info["build"]["mcu"]
                name = board_info["name"]
                board = fname[:-5]
                variant = mcu.upper()
                boards[board] = {
                    "name": name,
                    "variant": f"VARIANT_{variant}",
                }
        return boards


TEMPLATE = """    "%s": {
        "name": "%s",
        "variant": %s,
    },
"""


def main():
    boards = get_boards()
    # open boards.py, delete existing BOARDS variable and write the new boards dict
    boards_file_path = os.path.join(
        os.path.dirname(__file__), "..", "esphome", "components", "esp32", "boards.py"
    )
    with open(boards_file_path, encoding="UTF-8") as f:
        lines = f.readlines()

    with open(boards_file_path, "w", encoding="UTF-8") as f:
        for line in lines:
            if line.startswith("BOARDS = {"):
                f.write("BOARDS = {\n")
                f.writelines(
                    TEMPLATE % (board, info["name"], info["variant"])
                    for board, info in sorted(boards.items())
                )
                f.write("}\n")
                break

            f.write(line)


if __name__ == "__main__":
    main()
    print("ESP32 boards updated successfully.")
