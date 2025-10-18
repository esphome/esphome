#!/usr/bin/env python3
"""Extract memory usage statistics from ESPHome build output.

This script parses the PlatformIO build output to extract RAM and flash
usage statistics for a compiled component. It's used by the CI workflow to
compare memory usage between branches.

The script reads compile output from stdin and looks for the standard
PlatformIO output format:
    RAM:   [====      ]  36.1% (used 29548 bytes from 81920 bytes)
    Flash: [===       ]  34.0% (used 348511 bytes from 1023984 bytes)
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

# Add esphome to path
sys.path.insert(0, str(Path(__file__).parent.parent))

# pylint: disable=wrong-import-position
from script.ci_helpers import write_github_output


def extract_from_compile_output(output_text: str) -> tuple[int | None, int | None]:
    """Extract memory usage from PlatformIO compile output.

    Supports multiple builds (for component groups or isolated components).
    When test_build_components.py creates multiple builds, this sums the
    memory usage across all builds.

    Looks for lines like:
        RAM:   [====      ]  36.1% (used 29548 bytes from 81920 bytes)
        Flash: [===       ]  34.0% (used 348511 bytes from 1023984 bytes)

    Args:
        output_text: Compile output text (may contain multiple builds)

    Returns:
        Tuple of (total_ram_bytes, total_flash_bytes) or (None, None) if not found
    """
    # Find all RAM and Flash matches (may be multiple builds)
    ram_matches = re.findall(
        r"RAM:\s+\[.*?\]\s+\d+\.\d+%\s+\(used\s+(\d+)\s+bytes", output_text
    )
    flash_matches = re.findall(
        r"Flash:\s+\[.*?\]\s+\d+\.\d+%\s+\(used\s+(\d+)\s+bytes", output_text
    )

    if not ram_matches or not flash_matches:
        return None, None

    # Sum all builds (handles multiple component groups)
    total_ram = sum(int(match) for match in ram_matches)
    total_flash = sum(int(match) for match in flash_matches)

    return total_ram, total_flash


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Extract memory usage from ESPHome build output"
    )
    parser.add_argument(
        "--output-env",
        action="store_true",
        help="Output to GITHUB_OUTPUT environment file",
    )

    args = parser.parse_args()

    # Read compile output from stdin
    compile_output = sys.stdin.read()

    # Extract memory usage
    ram_bytes, flash_bytes = extract_from_compile_output(compile_output)

    if ram_bytes is None or flash_bytes is None:
        print("Failed to extract memory usage from compile output", file=sys.stderr)
        print("Expected lines like:", file=sys.stderr)
        print(
            "  RAM:   [====      ]  36.1% (used 29548 bytes from 81920 bytes)",
            file=sys.stderr,
        )
        print(
            "  Flash: [===       ]  34.0% (used 348511 bytes from 1023984 bytes)",
            file=sys.stderr,
        )
        return 1

    # Count how many builds were found
    num_builds = len(
        re.findall(
            r"RAM:\s+\[.*?\]\s+\d+\.\d+%\s+\(used\s+(\d+)\s+bytes", compile_output
        )
    )

    if num_builds > 1:
        print(
            f"Found {num_builds} builds - summing memory usage across all builds",
            file=sys.stderr,
        )

    print(f"Total RAM: {ram_bytes} bytes", file=sys.stderr)
    print(f"Total Flash: {flash_bytes} bytes", file=sys.stderr)

    if args.output_env:
        # Output to GitHub Actions
        write_github_output(
            {
                "ram_usage": ram_bytes,
                "flash_usage": flash_bytes,
            }
        )
    else:
        print(f"{ram_bytes},{flash_bytes}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
