#!/usr/bin/env python3
"""Extract memory usage statistics from ESPHome build output.

This script parses the PlatformIO build output to extract RAM and flash
usage statistics for a compiled component. It's used by the CI workflow to
compare memory usage between branches.

The script reads compile output from stdin and looks for the standard
PlatformIO output format:
    RAM:   [====      ]  36.1% (used 29548 bytes from 81920 bytes)
    Flash: [===       ]  34.0% (used 348511 bytes from 1023984 bytes)

Optionally performs detailed memory analysis if a build directory is provided.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

# Add esphome to path
sys.path.insert(0, str(Path(__file__).parent.parent))

# pylint: disable=wrong-import-position
from esphome.analyze_memory import MemoryAnalyzer
from esphome.platformio_api import IDEData
from script.ci_helpers import write_github_output

# Regex patterns for extracting memory usage from PlatformIO output
_RAM_PATTERN = re.compile(r"RAM:\s+\[.*?\]\s+\d+\.\d+%\s+\(used\s+(\d+)\s+bytes")
_FLASH_PATTERN = re.compile(r"Flash:\s+\[.*?\]\s+\d+\.\d+%\s+\(used\s+(\d+)\s+bytes")
_BUILD_PATH_PATTERN = re.compile(r"Build path: (.+)")


def extract_from_compile_output(
    output_text: str,
) -> tuple[int | None, int | None, str | None]:
    """Extract memory usage and build directory from PlatformIO compile output.

    Supports multiple builds (for component groups or isolated components).
    When test_build_components.py creates multiple builds, this sums the
    memory usage across all builds.

    Looks for lines like:
        RAM:   [====      ]  36.1% (used 29548 bytes from 81920 bytes)
        Flash: [===       ]  34.0% (used 348511 bytes from 1023984 bytes)

    Also extracts build directory from lines like:
        INFO Compiling app... Build path: /path/to/build

    Args:
        output_text: Compile output text (may contain multiple builds)

    Returns:
        Tuple of (total_ram_bytes, total_flash_bytes, build_dir) or (None, None, None) if not found
    """
    # Find all RAM and Flash matches (may be multiple builds)
    ram_matches = _RAM_PATTERN.findall(output_text)
    flash_matches = _FLASH_PATTERN.findall(output_text)

    if not ram_matches or not flash_matches:
        return None, None, None

    # Sum all builds (handles multiple component groups)
    total_ram = sum(int(match) for match in ram_matches)
    total_flash = sum(int(match) for match in flash_matches)

    # Extract build directory from ESPHome's explicit build path output
    # Look for: INFO Compiling app... Build path: /path/to/build
    # Note: Multiple builds reuse the same build path (each overwrites the previous)
    build_dir = None
    if match := _BUILD_PATH_PATTERN.search(output_text):
        build_dir = match.group(1).strip()

    return total_ram, total_flash, build_dir


def run_detailed_analysis(build_dir: str) -> dict | None:
    """Run detailed memory analysis on build directory.

    Args:
        build_dir: Path to ESPHome build directory

    Returns:
        Dictionary with analysis results or None if analysis fails
    """
    build_path = Path(build_dir)
    if not build_path.exists():
        print(f"Build directory not found: {build_dir}", file=sys.stderr)
        return None

    # Find firmware.elf
    elf_path = None
    for elf_candidate in [
        build_path / "firmware.elf",
        build_path / ".pioenvs" / build_path.name / "firmware.elf",
    ]:
        if elf_candidate.exists():
            elf_path = str(elf_candidate)
            break

    if not elf_path:
        print(f"firmware.elf not found in {build_dir}", file=sys.stderr)
        return None

    # Find idedata.json - check multiple locations
    device_name = build_path.name
    idedata_candidates = [
        # In .pioenvs for test builds
        build_path / ".pioenvs" / device_name / "idedata.json",
        # In .esphome/idedata for regular builds
        Path.home() / ".esphome" / "idedata" / f"{device_name}.json",
        # Check parent directories for .esphome/idedata (for test_build_components)
        build_path.parent.parent.parent / "idedata" / f"{device_name}.json",
    ]

    idedata = None
    for idedata_path in idedata_candidates:
        if not idedata_path.exists():
            continue
        try:
            with open(idedata_path, encoding="utf-8") as f:
                raw_data = json.load(f)
            idedata = IDEData(raw_data)
            print(f"Loaded idedata from: {idedata_path}", file=sys.stderr)
            break
        except (json.JSONDecodeError, OSError) as e:
            print(
                f"Warning: Failed to load idedata from {idedata_path}: {e}",
                file=sys.stderr,
            )

    analyzer = MemoryAnalyzer(elf_path, idedata=idedata)
    components = analyzer.analyze()

    # Convert to JSON-serializable format
    result = {
        "components": {
            name: {
                "text": mem.text_size,
                "rodata": mem.rodata_size,
                "data": mem.data_size,
                "bss": mem.bss_size,
                "flash_total": mem.flash_total,
                "ram_total": mem.ram_total,
                "symbol_count": mem.symbol_count,
            }
            for name, mem in components.items()
        },
        "symbols": {},
    }

    # Build symbol map
    for section in analyzer.sections.values():
        for symbol_name, size, _ in section.symbols:
            if size > 0:
                demangled = analyzer._demangle_symbol(symbol_name)
                result["symbols"][demangled] = size

    return result


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
    parser.add_argument(
        "--build-dir",
        help="Optional build directory for detailed memory analysis (overrides auto-detection)",
    )
    parser.add_argument(
        "--output-json",
        help="Optional path to save detailed analysis JSON",
    )
    parser.add_argument(
        "--output-build-dir",
        help="Optional path to write the detected build directory",
    )

    args = parser.parse_args()

    # Read compile output from stdin
    compile_output = sys.stdin.read()

    # Extract memory usage and build directory
    ram_bytes, flash_bytes, detected_build_dir = extract_from_compile_output(
        compile_output
    )

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
    num_builds = len(_RAM_PATTERN.findall(compile_output))

    if num_builds > 1:
        print(
            f"Found {num_builds} builds - summing memory usage across all builds",
            file=sys.stderr,
        )
        print(
            "WARNING: Detailed analysis will only cover the last build",
            file=sys.stderr,
        )

    print(f"Total RAM: {ram_bytes} bytes", file=sys.stderr)
    print(f"Total Flash: {flash_bytes} bytes", file=sys.stderr)

    # Determine which build directory to use (explicit arg overrides auto-detection)
    build_dir = args.build_dir or detected_build_dir

    if detected_build_dir:
        print(f"Detected build directory: {detected_build_dir}", file=sys.stderr)
        if num_builds > 1:
            print(
                f"  (using last of {num_builds} builds for detailed analysis)",
                file=sys.stderr,
            )

    # Write build directory to file if requested
    if args.output_build_dir and build_dir:
        build_dir_path = Path(args.output_build_dir)
        build_dir_path.parent.mkdir(parents=True, exist_ok=True)
        build_dir_path.write_text(build_dir)
        print(f"Wrote build directory to {args.output_build_dir}", file=sys.stderr)

    # Run detailed analysis if build directory available
    detailed_analysis = None
    if build_dir:
        print(f"Running detailed analysis on {build_dir}", file=sys.stderr)
        detailed_analysis = run_detailed_analysis(build_dir)

    # Save JSON output if requested
    if args.output_json:
        output_data = {
            "ram_bytes": ram_bytes,
            "flash_bytes": flash_bytes,
            "detailed_analysis": detailed_analysis,
        }

        output_path = Path(args.output_json)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(output_data, f, indent=2)
        print(f"Saved analysis to {args.output_json}", file=sys.stderr)

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
