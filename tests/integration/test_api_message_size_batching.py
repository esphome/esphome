"""Integration test for API batching with various message sizes."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, NumberInfo, SelectInfo, TextInfo, TextSensorInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


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
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "message-size-batching-test"

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

        # Collect all select entity object_ids for error messages
        select_ids = [s.object_id for s in selects]

        # Find our specific test entities
        small_select = None
        medium_select = None
        large_select = None

        for select in selects:
            if select.object_id == "small_select":
                small_select = select
            elif select.object_id == "medium_select":
                medium_select = select
            elif (
                select.object_id
                == "large_select_with_many_options_to_create_larger_payload"
            ):
                large_select = select

        assert small_select is not None, (
            f"Could not find small_select entity. Found: {select_ids}"
        )
        assert medium_select is not None, (
            f"Could not find medium_select entity. Found: {select_ids}"
        )
        assert large_select is not None, (
            f"Could not find large_select entity. Found: {select_ids}"
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

        # Collect all text sensor object_ids for error messages
        text_sensor_ids = [t.object_id for t in text_sensors]

        # Verify text sensors with different value lengths
        short_text_sensor = None
        medium_text_sensor = None
        long_text_sensor = None

        for text_sensor in text_sensors:
            if text_sensor.object_id == "short_text_sensor":
                short_text_sensor = text_sensor
            elif text_sensor.object_id == "medium_text_sensor":
                medium_text_sensor = text_sensor
            elif text_sensor.object_id == "long_text_sensor_with_very_long_value":
                long_text_sensor = text_sensor

        assert short_text_sensor is not None, (
            f"Could not find short_text_sensor. Found: {text_sensor_ids}"
        )
        assert medium_text_sensor is not None, (
            f"Could not find medium_text_sensor. Found: {text_sensor_ids}"
        )
        assert long_text_sensor is not None, (
            f"Could not find long_text_sensor. Found: {text_sensor_ids}"
        )

        # Check text input which can have a long max_length
        text_input = None
        text_input_ids = [t.object_id for t in text_inputs]

        for ti in text_inputs:
            if ti.object_id == "test_text_input":
                text_input = ti
                break

        assert text_input is not None, (
            f"Could not find test_text_input. Found: {text_input_ids}"
        )
        assert text_input.max_length == 255, (
            f"Expected max_length 255, got {text_input.max_length}"
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
        try:
            await asyncio.wait_for(states_future, timeout=5.0)
        except TimeoutError:
            missing_keys = expected_keys - received_keys
            pytest.fail(
                f"Did not receive states from all entities within 5 seconds. "
                f"Missing keys: {missing_keys}, "
                f"Received {len(received_keys)} of {len(expected_keys)} expected states"
            )

        # Verify we received states from all entities
        assert expected_keys.issubset(received_keys)

        # Check that various message sizes were handled correctly
        # Small messages (4-byte header): type < 128, payload < 128
        # Medium messages (5-byte header): type < 128, payload 128-16383 OR type 128+, payload < 128
        # Large messages (6-byte header): type 128+, payload 128-16383
