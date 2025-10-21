"""Integration test for CachedGPIOExpander to ensure correct behavior."""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_gpio_expander_cache(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test gpio_expander::CachedGpioExpander correctly calls hardware functions."""
    # Get the path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    logs_done = asyncio.Event()

    # Patterns to match in logs - match any variation of digital_read
    read_hw_pattern = re.compile(r"(?:uint16_)?digital_read_hw pin=(\d+)")
    read_cache_pattern = re.compile(r"(?:uint16_)?digital_read_cache pin=(\d+)")

    # Keep specific patterns for building the expected order
    digital_read_hw_pattern = re.compile(r"^digital_read_hw pin=(\d+)")
    digital_read_cache_pattern = re.compile(r"^digital_read_cache pin=(\d+)")
    uint16_read_hw_pattern = re.compile(r"^uint16_digital_read_hw pin=(\d+)")
    uint16_read_cache_pattern = re.compile(r"^uint16_digital_read_cache pin=(\d+)")

    # ensure logs are in the expected order
    log_order = [
        (digital_read_hw_pattern, 0),
        [(digital_read_cache_pattern, i) for i in range(0, 8)],
        (digital_read_hw_pattern, 8),
        [(digital_read_cache_pattern, i) for i in range(8, 16)],
        (digital_read_hw_pattern, 16),
        [(digital_read_cache_pattern, i) for i in range(16, 24)],
        (digital_read_hw_pattern, 24),
        [(digital_read_cache_pattern, i) for i in range(24, 32)],
        (digital_read_hw_pattern, 3),
        (digital_read_cache_pattern, 3),
        (digital_read_hw_pattern, 3),
        (digital_read_cache_pattern, 3),
        (digital_read_cache_pattern, 4),
        (digital_read_hw_pattern, 3),
        (digital_read_cache_pattern, 3),
        (digital_read_hw_pattern, 10),
        (digital_read_cache_pattern, 10),
        # full cache reset here for testing
        (digital_read_hw_pattern, 15),
        (digital_read_cache_pattern, 15),
        (digital_read_cache_pattern, 14),
        (digital_read_hw_pattern, 14),
        (digital_read_cache_pattern, 14),
        # uint16_t component tests (single bank of 16 pins)
        (uint16_read_hw_pattern, 0),  # First pin triggers hw read
        [
            (uint16_read_cache_pattern, i) for i in range(0, 16)
        ],  # All 16 pins return via cache
        # After cache reset
        (uint16_read_hw_pattern, 5),  # First read after reset triggers hw
        (uint16_read_cache_pattern, 5),
        (uint16_read_cache_pattern, 10),  # These use cache (same bank)
        (uint16_read_cache_pattern, 15),
        (uint16_read_cache_pattern, 0),
    ]
    # Flatten the log order for easier processing
    log_order: list[tuple[re.Pattern, int]] = [
        item
        for sublist in log_order
        for item in (sublist if isinstance(sublist, list) else [sublist])
    ]

    index = 0

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        nonlocal index
        if logs_done.is_set():
            return

        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)

        # Extract just the log message part (after the log level)
        msg = clean_line.split(": ", 1)[-1] if ": " in clean_line else clean_line

        # Check if this line contains a read operation we're tracking
        if read_hw_pattern.search(msg) or read_cache_pattern.search(msg):
            if index >= len(log_order):
                print(f"Received unexpected log line: {msg}")
                logs_done.set()
                return

            pattern, expected_pin = log_order[index]
            match = pattern.search(msg)

            if not match:
                print(f"Log line did not match next expected pattern: {msg}")
                print(f"Expected pattern: {pattern.pattern}")
                logs_done.set()
                return

            pin = int(match.group(1))
            if pin != expected_pin:
                print(f"Unexpected pin number. Expected {expected_pin}, got {pin}")
                logs_done.set()
                return

            index += 1

        elif "DONE_UINT16" in clean_line:
            # uint16 component is done, check if we've seen all expected logs
            if index == len(log_order):
                logs_done.set()

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "gpio-expander-cache"

        try:
            await asyncio.wait_for(logs_done.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail("Timeout waiting for logs to complete")

        assert index == len(log_order), (
            f"Expected {len(log_order)} log entries, but got {index}"
        )
