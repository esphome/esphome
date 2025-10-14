#!/usr/bin/env python3
"""Detect if a PR changes exactly one component for memory impact analysis.

This script is used by the CI workflow to determine if a PR should trigger
memory impact analysis. The analysis only runs when:
1. Exactly one component has changed (not counting core changes)
2. The component has at least one test configuration

The script outputs GitHub Actions environment variables to control the workflow.
"""

from __future__ import annotations

from pathlib import Path
import sys

# Add esphome to path
sys.path.insert(0, str(Path(__file__).parent.parent))

# pylint: disable=wrong-import-position
from script.ci_helpers import write_github_output
from script.helpers import ESPHOME_COMPONENTS_PATH, changed_files

# Platform preference order for memory impact analysis
# Ordered by production relevance and memory constraint importance
PLATFORM_PREFERENCE = [
    "esp32-idf",  # Primary ESP32 IDF platform
    "esp32-c3-idf",  # ESP32-C3 IDF
    "esp32-c6-idf",  # ESP32-C6 IDF
    "esp32-s2-idf",  # ESP32-S2 IDF
    "esp32-s3-idf",  # ESP32-S3 IDF
    "esp32-c2-idf",  # ESP32-C2 IDF
    "esp32-c5-idf",  # ESP32-C5 IDF
    "esp32-h2-idf",  # ESP32-H2 IDF
    "esp32-p4-idf",  # ESP32-P4 IDF
    "esp8266-ard",  # ESP8266 Arduino (memory constrained)
    "esp32-ard",  # ESP32 Arduino
    "esp32-c3-ard",  # ESP32-C3 Arduino
    "esp32-s2-ard",  # ESP32-S2 Arduino
    "esp32-s3-ard",  # ESP32-S3 Arduino
    "bk72xx-ard",  # BK72xx Arduino
    "rp2040-ard",  # RP2040 Arduino
    "nrf52-adafruit",  # nRF52 Adafruit
    "host",  # Host platform (development/testing)
]


def find_test_for_component(component: str) -> tuple[str | None, str | None]:
    """Find a test configuration for the given component.

    Prefers platforms based on PLATFORM_PREFERENCE order.

    Args:
        component: Component name

    Returns:
        Tuple of (test_file_name, platform) or (None, None) if no test found
    """
    tests_dir = Path(__file__).parent.parent / "tests" / "components" / component

    if not tests_dir.exists():
        return None, None

    # Look for test files
    test_files = list(tests_dir.glob("test.*.yaml"))
    if not test_files:
        return None, None

    # Try each preferred platform in order
    for preferred_platform in PLATFORM_PREFERENCE:
        for test_file in test_files:
            parts = test_file.stem.split(".")
            if len(parts) >= 2:
                platform = parts[1]
                if platform == preferred_platform:
                    return test_file.name, platform

    # Fall back to first test file
    test_file = test_files[0]
    parts = test_file.stem.split(".")
    platform = parts[1] if len(parts) >= 2 else "esp32-idf"
    return test_file.name, platform


def detect_single_component_change() -> None:
    """Detect if exactly one component changed and output GitHub Actions variables."""
    files = changed_files()

    # Find all changed components (excluding core)
    changed_components = set()

    for file in files:
        if file.startswith(ESPHOME_COMPONENTS_PATH):
            parts = file.split("/")
            if len(parts) >= 3:
                component = parts[2]
                # Skip base bus components as they're used across many builds
                if component not in ["i2c", "spi", "uart", "modbus"]:
                    changed_components.add(component)

    # Only proceed if exactly one component changed
    if len(changed_components) != 1:
        print(
            f"Found {len(changed_components)} component(s) changed, skipping memory analysis"
        )
        write_github_output({"should_run": "false"})
        return

    component = list(changed_components)[0]
    print(f"Detected single component change: {component}")

    # Find a test configuration for this component
    test_file, platform = find_test_for_component(component)

    if not test_file:
        print(f"No test configuration found for {component}, skipping memory analysis")
        write_github_output({"should_run": "false"})
        return

    print(f"Found test: {test_file} for platform: {platform}")
    print("Memory impact analysis will run")

    write_github_output(
        {
            "should_run": "true",
            "component": component,
            "test_file": test_file,
            "platform": platform,
        }
    )


if __name__ == "__main__":
    detect_single_component_change()
