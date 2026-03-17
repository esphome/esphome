#!/usr/bin/env python3
"""Set up CodSpeed's google_benchmark fork as a PlatformIO library.

CodSpeed requires their codspeed-cpp fork for CPU simulation instrumentation.
This script clones the repo and creates a PlatformIO-compatible library.json
that combines the google_benchmark and codspeed core sources.

Usage:
    python script/setup_codspeed_lib.py [--output-dir DIR]

Prints the PlatformIO library path (symlink:// URL) to stdout.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess

# Pin to a specific commit for reproducibility
CODSPEED_CPP_REPO = "https://github.com/CodSpeedHQ/codspeed-cpp.git"
CODSPEED_CPP_SHA = "d6b4111428ae1f1667fec9bff009522378d5d347"

DEFAULT_OUTPUT_DIR = "/tmp/codspeed-cpp"


def setup_codspeed_lib(output_dir: Path) -> str:
    """Clone codspeed-cpp and create PlatformIO library layout.

    Args:
        output_dir: Directory to clone into

    Returns:
        PlatformIO library path (symlink:// URL)
    """
    if not (output_dir / ".git").exists():
        subprocess.run(
            ["git", "clone", CODSPEED_CPP_REPO, str(output_dir)],
            check=True,
        )
    subprocess.run(
        ["git", "-C", str(output_dir), "checkout", CODSPEED_CPP_SHA],
        check=True,
        capture_output=True,
    )

    benchmark_dir = output_dir / "google_benchmark"
    core_dir = output_dir / "core"

    # Create library.json combining google_benchmark + codspeed core
    library_json = {
        "name": "benchmark",
        "version": "0.0.0",
        "build": {
            "flags": [
                f"-I{core_dir / 'include'}",
                f"-I{core_dir / 'instrument-hooks'}",
                "-DHAVE_STD_REGEX",
                "-DHAVE_STEADY_CLOCK",
                "-DBENCHMARK_STATIC_DEFINE",
            ],
            "srcFilter": [
                "+<src/*.cc>",
                f"+<{core_dir / 'src' / '*.cpp'}>",
            ],
            "includeDir": "include",
        },
    }

    (benchmark_dir / "library.json").write_text(
        json.dumps(library_json, indent=2) + "\n"
    )

    return f"symlink://{benchmark_dir}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(DEFAULT_OUTPUT_DIR),
        help=f"Directory to clone codspeed-cpp into (default: {DEFAULT_OUTPUT_DIR})",
    )
    args = parser.parse_args()

    lib_path = setup_codspeed_lib(args.output_dir)
    print(lib_path)


if __name__ == "__main__":
    main()
