"""Integration test for alarm control panel state transitions."""

from __future__ import annotations

import asyncio
import re

import aioesphomeapi
from aioesphomeapi import (
    AlarmControlPanelCommand,
    AlarmControlPanelEntityState,
    AlarmControlPanelInfo,
    AlarmControlPanelState,
    SwitchInfo,
)
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_alarm_control_panel_state_transitions(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test alarm control panel state transitions.

    This comprehensive test verifies all state transitions and listener callbacks:

    1. Basic arm/disarm sequences:
       - DISARMED -> ARMING -> ARMED_AWAY -> DISARMED
       - DISARMED -> ARMING -> ARMED_HOME -> DISARMED
       - DISARMED -> ARMING -> ARMED_NIGHT -> DISARMED

    2. Wrong code rejection

    3. Sensor triggering while armed:
       - ARMED_AWAY -> PENDING -> TRIGGERED (delayed sensor)
       - TRIGGERED -> ARMED_AWAY (auto-reset after trigger_time, fires on_cleared)

    4. Chime functionality:
       - Sensor open while DISARMED triggers on_chime

    5. Ready state:
       - Sensor state changes trigger on_ready
    """
    loop = asyncio.get_running_loop()

    # Track log messages for callback verification
    log_lines: list[str] = []
    chime_future: asyncio.Future[bool] = loop.create_future()
    ready_futures: list[asyncio.Future[bool]] = []
    cleared_future: asyncio.Future[bool] = loop.create_future()

    # Patterns to match log output from callbacks
    chime_pattern = re.compile(r"Chime activated")
    ready_pattern = re.compile(r"Sensors ready state changed")
    cleared_pattern = re.compile(r"Alarm cleared")

    def on_log_line(line: str) -> None:
        log_lines.append(line)
        if not chime_future.done() and chime_pattern.search(line):
            chime_future.set_result(True)
        if ready_pattern.search(line):
            # Create new future for each ready event
            for fut in ready_futures:
                if not fut.done():
                    fut.set_result(True)
                    break
        if not cleared_future.done() and cleared_pattern.search(line):
            cleared_future.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        # Find entities
        alarm_info: AlarmControlPanelInfo | None = None
        door_switch_info: SwitchInfo | None = None
        chime_switch_info: SwitchInfo | None = None

        for entity in entities:
            if isinstance(entity, AlarmControlPanelInfo):
                alarm_info = entity
            elif isinstance(entity, SwitchInfo):
                if entity.name == "Door Sensor Switch":
                    door_switch_info = entity
                elif entity.name == "Chime Sensor Switch":
                    chime_switch_info = entity

        assert alarm_info is not None, "Alarm control panel not found"
        assert door_switch_info is not None, "Door sensor switch not found"
        assert chime_switch_info is not None, "Chime sensor switch not found"

        # Track state changes
        states_received: list[AlarmControlPanelState] = []
        state_event = asyncio.Event()

        def on_state(state: aioesphomeapi.EntityState) -> None:
            if (
                isinstance(state, AlarmControlPanelEntityState)
                and state.key == alarm_info.key
            ):
                states_received.append(state.state)
                state_event.set()

        # Use InitialStateHelper to handle initial state broadcast
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for initial states from all entities
        await initial_state_helper.wait_for_initial_states()

        # Verify alarm panel started in DISARMED state
        initial_alarm_state = initial_state_helper.initial_states.get(alarm_info.key)
        assert initial_alarm_state is not None, "No initial alarm state received"
        assert isinstance(initial_alarm_state, AlarmControlPanelEntityState)
        assert initial_alarm_state.state == AlarmControlPanelState.DISARMED

        # Helper to wait for specific state
        async def wait_for_state(
            expected: AlarmControlPanelState, timeout: float = 5.0
        ) -> None:
            deadline = loop.time() + timeout
            while True:
                remaining = deadline - loop.time()
                if remaining <= 0:
                    raise TimeoutError(
                        f"Timeout waiting for state {expected}, "
                        f"last state: {states_received[-1] if states_received else 'none'}"
                    )
                await asyncio.wait_for(state_event.wait(), timeout=remaining)
                state_event.clear()
                if states_received[-1] == expected:
                    return

        # ===== Test wrong code rejection =====
        client.alarm_control_panel_command(
            alarm_info.key,
            AlarmControlPanelCommand.ARM_AWAY,
            code="0000",  # Wrong code
        )

        # Should NOT transition - wait a bit and verify no state changes
        with pytest.raises(asyncio.TimeoutError):
            await asyncio.wait_for(state_event.wait(), timeout=0.5)
        # No state changes should have occurred (list is empty)
        assert len(states_received) == 0, f"Unexpected state changes: {states_received}"

        # ===== Test ARM_AWAY sequence =====
        client.alarm_control_panel_command(
            alarm_info.key,
            AlarmControlPanelCommand.ARM_AWAY,
            code="1234",
        )
        await wait_for_state(AlarmControlPanelState.ARMING)
        await wait_for_state(AlarmControlPanelState.ARMED_AWAY)

        # Disarm
        client.alarm_control_panel_command(
            alarm_info.key,
            AlarmControlPanelCommand.DISARM,
            code="1234",
        )
        await wait_for_state(AlarmControlPanelState.DISARMED)

        # ===== Test ARM_HOME sequence =====
        client.alarm_control_panel_command(
            alarm_info.key,
            AlarmControlPanelCommand.ARM_HOME,
            code="1234",
        )
        await wait_for_state(AlarmControlPanelState.ARMING)
        await wait_for_state(AlarmControlPanelState.ARMED_HOME)

        # Disarm
        client.alarm_control_panel_command(
            alarm_info.key,
            AlarmControlPanelCommand.DISARM,
            code="1234",
        )
        await wait_for_state(AlarmControlPanelState.DISARMED)

        # ===== Test ARM_NIGHT sequence =====
        client.alarm_control_panel_command(
            alarm_info.key,
            AlarmControlPanelCommand.ARM_NIGHT,
            code="1234",
        )
        await wait_for_state(AlarmControlPanelState.ARMING)
        await wait_for_state(AlarmControlPanelState.ARMED_NIGHT)

        # Disarm
        client.alarm_control_panel_command(
            alarm_info.key,
            AlarmControlPanelCommand.DISARM,
            code="1234",
        )
        await wait_for_state(AlarmControlPanelState.DISARMED)

        # Verify basic state sequence (initial DISARMED is handled by InitialStateHelper)
        expected_states = [
            AlarmControlPanelState.ARMING,  # Arm away
            AlarmControlPanelState.ARMED_AWAY,
            AlarmControlPanelState.DISARMED,
            AlarmControlPanelState.ARMING,  # Arm home
            AlarmControlPanelState.ARMED_HOME,
            AlarmControlPanelState.DISARMED,
            AlarmControlPanelState.ARMING,  # Arm night
            AlarmControlPanelState.ARMED_NIGHT,
            AlarmControlPanelState.DISARMED,
        ]
        assert states_received == expected_states, (
            f"State sequence mismatch.\nExpected: {expected_states}\n"
            f"Got: {states_received}"
        )

        # ===== Test PENDING -> TRIGGERED -> CLEARED sequence =====
        # This tests on_pending, on_triggered, and on_cleared callbacks

        # Arm away first
        client.alarm_control_panel_command(
            alarm_info.key,
            AlarmControlPanelCommand.ARM_AWAY,
            code="1234",
        )
        await wait_for_state(AlarmControlPanelState.ARMING)
        await wait_for_state(AlarmControlPanelState.ARMED_AWAY)

        # Trip the door sensor (delayed mode triggers PENDING first)
        client.switch_command(door_switch_info.key, True)

        # Should go to PENDING (delayed sensor)
        await wait_for_state(AlarmControlPanelState.PENDING)

        # Should go to TRIGGERED after pending_time (50ms)
        await wait_for_state(AlarmControlPanelState.TRIGGERED)

        # Close the sensor
        client.switch_command(door_switch_info.key, False)

        # Wait for trigger_time to expire and auto-reset (100ms)
        # The alarm should go back to ARMED_AWAY after trigger_time
        # This transition FROM TRIGGERED fires on_cleared
        await wait_for_state(AlarmControlPanelState.ARMED_AWAY, timeout=2.0)

        # Verify on_cleared was logged
        try:
            await asyncio.wait_for(cleared_future, timeout=1.0)
        except TimeoutError:
            pytest.fail(f"on_cleared callback not fired. Log lines: {log_lines[-20:]}")

        # Disarm
        client.alarm_control_panel_command(
            alarm_info.key,
            AlarmControlPanelCommand.DISARM,
            code="1234",
        )
        await wait_for_state(AlarmControlPanelState.DISARMED)

        # Verify trigger sequence was added
        assert AlarmControlPanelState.PENDING in states_received
        assert AlarmControlPanelState.TRIGGERED in states_received

        # ===== Test chime (sensor open while disarmed) =====
        # The chime_sensor has chime: true, so opening it while disarmed
        # should trigger on_chime callback

        # We're currently DISARMED - open the chime sensor
        client.switch_command(chime_switch_info.key, True)

        # Wait for chime callback to be logged
        try:
            await asyncio.wait_for(chime_future, timeout=2.0)
        except TimeoutError:
            pytest.fail(f"on_chime callback not fired. Log lines: {log_lines[-20:]}")

        # Close the chime sensor
        client.switch_command(chime_switch_info.key, False)

        # ===== Test ready state changes =====
        # Opening/closing sensors while disarmed affects ready state
        # The on_ready callback fires when sensors_ready changes

        # Set up futures for ready state changes
        ready_future_1: asyncio.Future[bool] = loop.create_future()
        ready_future_2: asyncio.Future[bool] = loop.create_future()
        ready_futures.extend([ready_future_1, ready_future_2])

        # Open door sensor (makes alarm not ready)
        client.switch_command(door_switch_info.key, True)

        # Wait for first on_ready callback (not ready)
        try:
            await asyncio.wait_for(ready_future_1, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"on_ready callback not fired when sensor opened. "
                f"Log lines: {log_lines[-20:]}"
            )

        # Close door sensor (makes alarm ready again)
        client.switch_command(door_switch_info.key, False)

        # Wait for second on_ready callback (ready)
        try:
            await asyncio.wait_for(ready_future_2, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"on_ready callback not fired when sensor closed. "
                f"Log lines: {log_lines[-20:]}"
            )

        # Final state should still be DISARMED
        assert states_received[-1] == AlarmControlPanelState.DISARMED
