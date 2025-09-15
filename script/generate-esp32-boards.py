#!/usr/bin/env python3

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from esphome.components.esp32 import ESP_IDF_PLATFORM_VERSION as ver
from esphome.helpers import write_file_if_changed

version_str = f"{ver.major}.{ver.minor:02d}.{ver.patch:02d}"
root = Path(__file__).parent.parent
boards_file_path = root / "esphome" / "components" / "esp32" / "boards.py"


def get_boards():
    with tempfile.TemporaryDirectory() as tempdir:
        subprocess.run(
            [
                "git",
                "clone",
                "-q",
                "-c",
                "advice.detachedHead=false",
                "--depth",
                "1",
                "--branch",
                f"{ver.major}.{ver.minor:02d}.{ver.patch:02d}",
                "https://github.com/pioarduino/platform-espressif32",
                tempdir,
            ],
            check=True,
        )
        boards_directory = Path(tempdir) / "boards"
        boards = {}
        for fname in boards_directory.glob("*.json"):
            with fname.open(encoding="utf-8") as f:
                board_info = json.load(f)
                mcu = board_info["build"]["mcu"]
                name = board_info["name"]
                board = fname.stem
                variant = mcu.upper()
                boards[board] = {
                    "name": name,
                    "variant": f"VARIANT_{variant}",
                }
        return boards


TEMPLATE = """    "%s": {
        "name": "%s",
        "variant": %s,
    },"""


def main(check: bool):
    boards = get_boards()
    # open boards.py, delete existing BOARDS variable and write the new boards dict
    existing_content = boards_file_path.read_text(encoding="UTF-8")

    parts: list[str] = []
    for line in existing_content.splitlines():
        if line == "BOARDS = {":
            parts.append(line)
            parts.extend(
                TEMPLATE % (board, info["name"], info["variant"])
                for board, info in sorted(boards.items())
            )
            parts.append("}")
            parts.append("# DO NOT ADD ANYTHING BELOW THIS LINE")
            break

        parts.append(line)

    parts.append("")
    content = "\n".join(parts)

    if check:
        if existing_content != content:
            print("boards.py file is not up to date.")
            print("Please run `script/generate-esp32-boards.py`")
            sys.exit(1)
        print("boards.py file is up to date")
    elif write_file_if_changed(boards_file_path, content):
        print("ESP32 boards updated successfully.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        help="Check if the boards.py file is up to date.",
        action="store_true",
    )
    args = parser.parse_args()
    main(args.check)
