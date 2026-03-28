#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

from esphome.components.rp2040 import RECOMMENDED_ARDUINO_FRAMEWORK_VERSION
from esphome.components.rp2040.generate_boards import generate
from esphome.helpers import write_file_if_changed

ver = RECOMMENDED_ARDUINO_FRAMEWORK_VERSION
version_tag: str = f"{ver.major}.{ver.minor}.{ver.patch}"
root: Path = Path(__file__).parent.parent
boards_file_path: Path = root / "esphome" / "components" / "rp2040" / "boards.py"


def main(check: bool) -> None:
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
                version_tag,
                "https://github.com/earlephilhower/arduino-pico",
                tempdir,
            ],
            check=True,
        )

        content: str = generate(Path(tempdir))

    if check:
        existing_content: str = boards_file_path.read_text(encoding="utf-8")
        if existing_content != content:
            print("esphome/components/rp2040/boards.py is not up to date.")
            print("Please run `script/generate-rp2040-boards.py`")
            sys.exit(1)
        print("esphome/components/rp2040/boards.py is up to date")
    elif write_file_if_changed(boards_file_path, content):
        print("RP2040 boards updated successfully.")


if __name__ == "__main__":
    parser: argparse.ArgumentParser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        help="Check if the boards.py file is up to date.",
        action="store_true",
    )
    args: argparse.Namespace = parser.parse_args()
    main(args.check)
