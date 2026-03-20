"""Integration test for API with VERY_VERBOSE logging to verify no buffer corruption."""

from __future__ import annotations

import asyncio
from typing import Any

from aioesphomeapi import LogLevel, SensorInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_vv_logging(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that VERY_VERBOSE logging doesn't cause buffer corruption with API messages."""

    # Track that we're receiving VV log messages and sensor updates
    vv_logs_received = 0
    sensor_updates_received = 0
    errors_detected = []

    def on_log(msg: Any) -> None:
        """Capture log messages."""
        nonlocal vv_logs_received
        # msg is a SubscribeLogsResponse object with 'message' attribute
        # The message field is always bytes
        message_text = msg.message.decode("utf-8", errors="replace")

        # Only count VV logs specifically
        if "[VV]" in message_text:
            vv_logs_received += 1

        # Check for assertion or error messages
        if "assert" in message_text.lower() or "error" in message_text.lower():
            errors_detected.append(message_text)

    # Write, compile and run the ESPHome device
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Subscribe to VERY_VERBOSE logs - this enables the code path that could cause corruption
        client.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_VERY_VERBOSE)

        # Wait for device to be ready
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "vv-logging-test"

        # Subscribe to sensor states
        states = {}

        def on_state(state):
            nonlocal sensor_updates_received
            sensor_updates_received += 1
            states[state.key] = state

        client.subscribe_states(on_state)

        # List entities to find our test sensors
        entity_info, _ = await client.list_entities_services()

        # Count sensors
        sensor_count = sum(1 for e in entity_info if isinstance(e, SensorInfo))
        assert sensor_count >= 10, f"Expected at least 10 sensors, got {sensor_count}"

        # Wait for sensor updates to flow with VV logging active
        # The sensors update every 50ms, so we should get many updates
        await asyncio.sleep(0.25)

        # Verify we received both VV logs and sensor updates
        assert vv_logs_received > 0, "Expected to receive VERY_VERBOSE log messages"
        assert sensor_updates_received > 10, (
            f"Expected many sensor updates, got {sensor_updates_received}"
        )

        # Check for any errors
        if errors_detected:
            pytest.fail(f"Errors detected during test: {errors_detected}")

        # The test passes if we didn't hit any assertions or buffer corruption
