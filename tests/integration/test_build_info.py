"""Integration test for build_info values."""

from __future__ import annotations

import asyncio
from datetime import datetime
import re
import time

from aioesphomeapi import EntityState, TextSensorState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_build_info(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that build_info values are sane."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "build-info-test"

        # Verify compilation_time from device_info is present and parseable
        # The format is ISO 8601 with timezone: "YYYY-MM-DD HH:MM:SS +ZZZZ"
        compilation_time = device_info.compilation_time
        assert compilation_time is not None

        # Validate the ISO format: "YYYY-MM-DD HH:MM:SS +ZZZZ"
        parsed = datetime.strptime(compilation_time, "%Y-%m-%d %H:%M:%S %z")
        assert parsed.year >= time.localtime().tm_year

        # Get entities
        entities, _ = await client.list_entities_services()

        # Find our text sensors by object_id
        config_hash_entity = next(
            (e for e in entities if e.object_id == "config_hash"), None
        )
        build_time_entity = next(
            (e for e in entities if e.object_id == "build_time"), None
        )
        build_time_str_entity = next(
            (e for e in entities if e.object_id == "build_time_string"), None
        )

        assert config_hash_entity is not None, "Config Hash sensor not found"
        assert build_time_entity is not None, "Build Time sensor not found"
        assert build_time_str_entity is not None, "Build Time String sensor not found"

        # Wait for all three text sensors to have valid states
        loop = asyncio.get_running_loop()
        states: dict[int, TextSensorState] = {}
        all_received = loop.create_future()
        expected_keys = {
            config_hash_entity.key,
            build_time_entity.key,
            build_time_str_entity.key,
        }

        def on_state(state: EntityState) -> None:
            if isinstance(state, TextSensorState) and not state.missing_state:
                states[state.key] = state
                if expected_keys <= states.keys() and not all_received.done():
                    all_received.set_result(True)

        client.subscribe_states(on_state)

        try:
            await asyncio.wait_for(all_received, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for text sensor states. Got: {list(states.keys())}"
            )

        config_hash_state = states[config_hash_entity.key]
        build_time_state = states[build_time_entity.key]
        build_time_str_state = states[build_time_str_entity.key]

        # Validate config_hash format (0x followed by 8 hex digits)
        config_hash = config_hash_state.state
        assert re.match(r"^0x[0-9a-f]{8}$", config_hash), (
            f"config_hash should be 0x followed by 8 hex digits, got: {config_hash}"
        )

        # Validate build_time is a reasonable Unix timestamp
        build_time = int(build_time_state.state)
        current_time = int(time.time())
        # Build time should be within last hour and not in the future
        assert build_time <= current_time, (
            f"build_time {build_time} should not be in the future (current: {current_time})"
        )
        assert build_time > current_time - 3600, (
            f"build_time {build_time} should be within the last hour"
        )

        # Validate build_time_str matches the new ISO format
        build_time_str = build_time_str_state.state
        # Format: "YYYY-MM-DD HH:MM:SS +ZZZZ"
        parsed_build_time = datetime.strptime(build_time_str, "%Y-%m-%d %H:%M:%S %z")
        assert parsed_build_time.year >= time.localtime().tm_year

        # Verify build_time_str matches what we get from build_time timestamp
        expected_str = time.strftime("%Y-%m-%d %H:%M:%S %z", time.localtime(build_time))
        assert build_time_str == expected_str, (
            f"build_time_str '{build_time_str}' should match timestamp '{expected_str}'"
        )

        # Verify compilation_time matches build_time_str (they should be the same)
        assert compilation_time == build_time_str, (
            f"compilation_time '{compilation_time}' should match "
            f"build_time_str '{build_time_str}'"
        )
