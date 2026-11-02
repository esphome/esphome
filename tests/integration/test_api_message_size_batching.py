"""Integration test for API batching with various message sizes."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, NumberInfo, SelectInfo, TextInfo, TextSensorInfo
import pytest

from .state_utils import assert_device_name, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_api_message_size_batching(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API can batch messages of various sizes correctly."""
    # Write, compile and run the ESPHome device, then connect to API
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can get device info
        await assert_device_name(client, "message-size-batching-test")

        # List entities - this will batch various sized messages together
        entity_info, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Count different entity types
        selects = []
        text_sensors = []
        text_inputs = []
        numbers = []
        other_entities = []

        for entity in entity_info:
            if isinstance(entity, SelectInfo):
                selects.append(entity)
            elif isinstance(entity, TextSensorInfo):
                text_sensors.append(entity)
            elif isinstance(entity, TextInfo):
                text_inputs.append(entity)
            elif isinstance(entity, NumberInfo):
                numbers.append(entity)
            else:
                other_entities.append(entity)

        # Verify we have our test entities - exact counts
        assert len(selects) == 3, (
            f"Expected exactly 3 select entities, got {len(selects)}"
        )
        assert len(text_sensors) == 3, (
            f"Expected exactly 3 text sensor entities, got {len(text_sensors)}"
        )
        assert len(text_inputs) == 1, (
            f"Expected exactly 1 text input entity, got {len(text_inputs)}"
        )

        # Find our specific test entities
        small_select = require_entity(selects, "small_select", exact=True)
        medium_select = require_entity(selects, "medium_select", exact=True)
        large_select = require_entity(
            selects,
            "large_select_with_many_options_to_create_larger_payload",
            exact=True,
        )

        # Verify the selects have the expected number of options
        assert len(small_select.options) == 2, (
            f"Expected 2 options for small_select, got {len(small_select.options)}"
        )
        assert len(medium_select.options) == 20, (
            f"Expected 20 options for medium_select, got {len(medium_select.options)}"
        )
        assert len(large_select.options) == 50, (
            f"Expected 50 options for large_select, got {len(large_select.options)}"
        )

        # Verify text sensors with different value lengths
        require_entity(text_sensors, "short_text_sensor", exact=True)
        require_entity(text_sensors, "medium_text_sensor", exact=True)
        require_entity(
            text_sensors, "long_text_sensor_with_very_long_value", exact=True
        )

        # Check text input which can have a long max_length
        text_input = require_entity(text_inputs, "test_text_input", exact=True)
        assert text_input.max_length == 255, (
            f"Expected max_length 255, got {text_input.max_length}"
        )
        assert text_input.pattern == "[A-Za-z0-9 ]+", (
            f"Expected pattern '[A-Za-z0-9 ]+', got '{text_input.pattern}'"
        )

        # Verify total entity count - messages of various sizes were batched successfully
        # We have: 3 selects + 3 text sensors + 1 text input + 1 number = 8 total
        total_entities = len(entity_info)
        assert total_entities == 8, f"Expected exactly 8 entities, got {total_entities}"

        # Check we have the expected entity types
        assert len(numbers) == 1, (
            f"Expected exactly 1 number entity, got {len(numbers)}"
        )
        assert len(other_entities) == 0, (
            f"Unexpected entity types found: {[type(e).__name__ for e in other_entities]}"
        )

        # Subscribe to state changes to verify batching works
        # Collect keys from entity info to know what states to expect
        expected_keys = {entity.key for entity in entity_info}
        assert len(expected_keys) == 8, (
            f"Expected 8 unique entity keys, got {len(expected_keys)}"
        )

        received_keys: set[int] = set()
        states_future: asyncio.Future[None] = loop.create_future()

        def on_state(state: EntityState) -> None:
            """Track when states are received."""
            received_keys.add(state.key)
            # Check if we've received states from all expected entities
            if expected_keys.issubset(received_keys) and not states_future.done():
                states_future.set_result(None)

        client.subscribe_states(on_state)

        # Wait for states with timeout
        await wait_or_fail(
            states_future,
            5.0,
            "Did not receive states from all entities",
            missing_keys=lambda: expected_keys - received_keys,
            received=lambda: f"{len(received_keys)} of {len(expected_keys)}",
        )

        # Verify we received states from all entities
        assert expected_keys.issubset(received_keys)

        # Check that various message sizes were handled correctly
        # Small messages (4-byte header): type < 128, payload < 128
        # Medium messages (5-byte header): type < 128, payload 128-16383 OR type 128+, payload < 128
        # Large messages (6-byte header): type 128+, payload 128-16383
