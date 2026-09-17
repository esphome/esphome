"""Integration test for the template time platform, including the sync option."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, SensorState, TextSensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction

# Timestamps are exposed as text sensors (rather than 32-bit float sensor states,
# which cannot represent a UNIX epoch exactly) so the exact lambda return value
# can be checked.
EXPECTED_TIMESTAMPS = {
    "valid_sync_timestamp": "1700000000",
    "no_sync_timestamp": "1700000200",
    "default_interval_timestamp": "1700000300",
}

# on_time_sync counters, incremented by the fixture's YAML automations.
COUNT_SENSOR_NAMES = [
    "valid_sync_count_sensor",
    "invalid_sync_count_sensor",
    "no_sync_count_sensor",
    "default_interval_sync_count_sensor",
]
# The device polls each time source and sensor on its own jittered schedule, so
# the counter can skip over any single value (e.g. 2 -> 4). Wait for it to pass
# a threshold instead of matching an exact transient value.
VALID_SYNC_COUNT_THRESHOLD = 3.0


@pytest.mark.asyncio
async def test_template_time(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Verify the template time platform's lambda evaluation and sync option."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()

        text_key_to_name = build_key_to_entity_mapping(
            entities, list(EXPECTED_TIMESTAMPS)
        )
        timestamp_events = {name: asyncio.Event() for name in EXPECTED_TIMESTAMPS}

        count_key_to_name = build_key_to_entity_mapping(entities, COUNT_SENSOR_NAMES)
        count_states: dict[str, list[float]] = {name: [] for name in COUNT_SENSOR_NAMES}
        valid_sync_threshold_event = asyncio.Event()

        def on_state(state: EntityState) -> None:
            if isinstance(state, TextSensorState) and not state.missing_state:
                name = text_key_to_name.get(state.key)
                if name is not None and state.state == EXPECTED_TIMESTAMPS[name]:
                    timestamp_events[name].set()
            elif isinstance(state, SensorState) and not state.missing_state:
                name = count_key_to_name.get(state.key)
                if name is not None:
                    count_states[name].append(state.state)
                    if (
                        name == "valid_sync_count_sensor"
                        and state.state >= VALID_SYNC_COUNT_THRESHOLD
                    ):
                        valid_sync_threshold_event.set()

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        # The lambda is evaluated for .now() regardless of the sync/update_interval
        # settings, so every time source reports its own fixed timestamp.
        for name, event in timestamp_events.items():
            try:
                await asyncio.wait_for(event.wait(), timeout=3.0)
            except TimeoutError:
                pytest.fail(
                    f"Timeout waiting for {name} to report the expected timestamp"
                )

        # sync: true with a valid epoch and an explicit, fast update_interval must
        # trigger on_time_sync repeatedly, not just once.
        try:
            await asyncio.wait_for(valid_sync_threshold_event.wait(), timeout=3.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for valid_sync_count_sensor to reach "
                f"{VALID_SYNC_COUNT_THRESHOLD}. Received: "
                f"{count_states['valid_sync_count_sensor']}"
            )

        # Give the other three sources some extra runtime to prove a negative:
        # with sync firing every 100ms, they would have incremented several
        # times over by now if on_time_sync fired for them too.
        await asyncio.sleep(1.0)
        for name in (
            "invalid_sync_count_sensor",
            "no_sync_count_sensor",
            "default_interval_sync_count_sensor",
        ):
            # sync: true with an always-invalid epoch, sync: false, and sync:
            # true with the default ("never") update_interval must never fire
            # on_time_sync.
            assert all(value == 0.0 for value in count_states[name]), (
                f"{name} must stay at 0, but reported: {count_states[name]}"
            )
