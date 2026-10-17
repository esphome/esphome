"""Integration test for the binary_sensor autorepeat filter.

Verifies that the autorepeat filter:

1. Passes the initial true through unchanged.
2. Begins oscillating after the configured ``delay`` while the source stays true.
3. Stops oscillating and emits a final false when the source goes false.

This exercises both scheduled timers in ``AutorepeatFilter`` (the per-step
``delay`` timer keyed off the filter ``this`` pointer and the on/off toggle
timer keyed off ``&active_timing_``).
"""

from __future__ import annotations

import asyncio

import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_binary_sensor_autorepeat_filter(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Drive the source true and verify the downstream sensor oscillates."""
    collector = SensorStateCollector(
        sensor_names=[],
        binary_sensor_names=["autorepeat_sensor"],
    )

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "test-autorepeat-filter"

        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        press_button = require_entity(entities, "press", description="Press button")
        release_button = require_entity(
            entities, "release", description="Release button"
        )

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        autorepeat_states = collector.binary_states["autorepeat_sensor"]

        # Press: source becomes true, autorepeat passes the initial true through
        # and then oscillates after the configured delay.
        # Configured timings: delay=200ms, time_on=100ms, time_off=100ms.
        # Expected within ~700ms:
        #   true (0ms), false (200ms), true (300ms), false (400ms),
        #   true (500ms), false (600ms)
        client.button_command(press_button.key)

        # Wait for at least 5 transitions to verify the oscillation pattern.
        oscillation_seen = collector.add_waiter(lambda: len(autorepeat_states) >= 5)
        try:
            await asyncio.wait_for(oscillation_seen, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Expected at least 5 autorepeat transitions, got {autorepeat_states}"
            )

        assert autorepeat_states[0] is True, (
            f"First transition should be the pass-through true, got {autorepeat_states}"
        )
        # After the initial true and the configured delay, the filter must
        # toggle false/true/false/... — verify the alternation pattern.
        for index, value in enumerate(autorepeat_states):
            expected = index % 2 == 0
            assert value is expected, (
                f"Expected alternating values starting with True, "
                f"got {autorepeat_states} (mismatch at index {index})"
            )

        # Release: source becomes false, autorepeat must cancel both timers
        # and settle on false. If the most recent oscillation was already
        # false, the binary sensor will dedup and not emit a new state event;
        # if it was true, exactly one final false transition arrives. Either
        # way, the steady state must be false and no further toggles should
        # arrive after a settle window longer than time_on + time_off.
        was_true_before_release = autorepeat_states[-1] is True
        before_count = len(autorepeat_states)
        client.button_command(release_button.key)

        if was_true_before_release:
            settle_seen = collector.add_waiter(
                lambda: len(autorepeat_states) > before_count
            )
            try:
                await asyncio.wait_for(settle_seen, timeout=2.0)
            except TimeoutError:
                pytest.fail("Timeout waiting for autorepeat to settle to false")
            assert autorepeat_states[-1] is False, (
                f"After release, final state should be False, got {autorepeat_states}"
            )

        steady_count = len(autorepeat_states)
        await asyncio.sleep(0.5)
        assert len(autorepeat_states) == steady_count, (
            f"Expected no further toggles after release, "
            f"got {autorepeat_states[steady_count:]}"
        )
        assert autorepeat_states[-1] is False, (
            f"Final autorepeat state should be False, got {autorepeat_states}"
        )
