"""Integration test for API batch_delay setting."""

from __future__ import annotations

import asyncio
import time

from aioesphomeapi import EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_batch_delay(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API with batch_delay set to 0ms - messages should be sent immediately without batching."""
    # Write, compile and run the ESPHome device, then connect to API
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can get device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "host-batch-delay-test"

        # Subscribe to state changes
        states: dict[int, EntityState] = {}
        state_timestamps: dict[int, float] = {}
        entity_count_future: asyncio.Future[int] = loop.create_future()

        def on_state(state: EntityState) -> None:
            """Track when states are received."""
            states[state.key] = state
            state_timestamps[state.key] = time.monotonic()
            # When we have received all expected entities, resolve the future
            if len(states) >= 7 and not entity_count_future.done():
                entity_count_future.set_result(len(states))

        client.subscribe_states(on_state)

        # Wait for states from all entities with timeout
        try:
            entity_count = await asyncio.wait_for(entity_count_future, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Did not receive states from at least 7 entities within 5 seconds. "
                f"Received {len(states)} states"
            )

        # Verify we received all states
        assert entity_count >= 7, f"Expected at least 7 entities, got {entity_count}"
        assert len(states) >= 7  # 3 sensors + 2 binary sensors + 2 switches

        # When batch_delay is 0ms, states are sent immediately without batching
        # This means each state arrives in its own packet, which may actually be slower
        # than batching due to network overhead
        if state_timestamps:
            first_timestamp = min(state_timestamps.values())
            last_timestamp = max(state_timestamps.values())
            time_spread = last_timestamp - first_timestamp

            # With batch_delay=0ms, states arrive individually which may take longer
            # We just verify they all arrive within a reasonable time
            assert time_spread < 1.0, f"States took {time_spread:.3f}s to arrive"

        # Also test list_entities - with batch_delay=0ms each entity is sent separately
        start_time = time.monotonic()
        entity_info, services = await client.list_entities_services()
        end_time = time.monotonic()

        list_time = end_time - start_time

        # Verify we got all expected entities
        assert len(entity_info) >= 7  # 3 sensors + 2 binary sensors + 2 switches

        # With batch_delay=0ms, listing sends each entity separately which may be slower
        assert list_time < 1.0, f"list_entities took {list_time:.3f}s"
