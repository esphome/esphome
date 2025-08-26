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

    # Patterns to match in logs
    digital_read_hw_pattern = re.compile(r"digital_read_hw pin=(\d+)")
    digital_read_cache_pattern = re.compile(r"digital_read_cache pin=(\d+)")

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

        if "digital_read" in clean_line:
            if index >= len(log_order):
                print(f"Received unexpected log line: {clean_line}")
                logs_done.set()
                return

            pattern, expected_pin = log_order[index]
            match = pattern.search(clean_line)

            if not match:
                print(f"Log line did not match next expected pattern: {clean_line}")
                logs_done.set()
                return

            pin = int(match.group(1))
            if pin != expected_pin:
                print(f"Unexpected pin number. Expected {expected_pin}, got {pin}")
                logs_done.set()
                return

            index += 1

        elif "DONE" in clean_line:
            # Check if we reached the end of the expected log entries
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
