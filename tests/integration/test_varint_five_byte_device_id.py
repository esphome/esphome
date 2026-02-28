"""Integration test for 5-byte varint parsing of device_id fields.

Device IDs are FNV hashes (uint32) that frequently exceed 2^28 (268435456),
requiring 5 varint bytes. This test verifies that:
1. The firmware correctly decodes 5-byte varint device_id in incoming commands
2. The firmware correctly encodes large device_id values in state responses
3. Switch commands with large device_id reach the correct entity
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, SwitchInfo, SwitchState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_varint_five_byte_device_id(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that device_id values requiring 5-byte varints parse correctly."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        device_info = await client.device_info()
        devices = device_info.devices
        assert len(devices) >= 2, f"Expected at least 2 devices, got {len(devices)}"

        # Verify at least one device_id exceeds the 4-byte varint boundary (2^28)
        large_ids = [d for d in devices if d.device_id >= (1 << 28)]
        assert len(large_ids) > 0, (
            "Expected at least one device_id >= 2^28 to exercise 5-byte varint path. "
            f"Got device_ids: {[d.device_id for d in devices]}"
        )

        # Get entities
        all_entities, _ = await client.list_entities_services()
        switch_entities = [e for e in all_entities if isinstance(e, SwitchInfo)]

        # Find switches named "Device Switch" — one per sub-device
        device_switches = [e for e in switch_entities if e.name == "Device Switch"]
        assert len(device_switches) == 2, (
            f"Expected 2 'Device Switch' entities, got {len(device_switches)}"
        )

        # Verify switches have different device_ids matching the sub-devices
        switch_device_ids = {s.device_id for s in device_switches}
        assert len(switch_device_ids) == 2, "Switches should have different device_ids"

        # Subscribe to states and wait for initial states
        loop = asyncio.get_running_loop()
        states: dict[tuple[int, int], EntityState] = {}
        switch_futures: dict[tuple[int, int], asyncio.Future[EntityState]] = {}
        initial_done: asyncio.Future[bool] = loop.create_future()

        def on_state(state: EntityState) -> None:
            key = (state.device_id, state.key)
            states[key] = state

            if len(states) >= 3 and not initial_done.done():
                initial_done.set_result(True)

            if initial_done.done() and key in switch_futures:
                fut = switch_futures[key]
                if not fut.done() and isinstance(state, SwitchState):
                    fut.set_result(state)

        client.subscribe_states(on_state)

        try:
            await asyncio.wait_for(initial_done, timeout=10.0)
        except TimeoutError:
            pytest.fail(
                f"Timed out waiting for initial states. Got {len(states)} states"
            )

        # Verify state responses contain correct large device_id values
        for device in devices:
            device_states = [
                s for (did, _), s in states.items() if did == device.device_id
            ]
            assert len(device_states) > 0, (
                f"No states received for device '{device.name}' "
                f"(device_id={device.device_id})"
            )

        # Test switch commands with large device_id varints —
        # this is the critical path: the client encodes device_id as a varint
        # in the SwitchCommandRequest, and the firmware must decode it correctly.
        for switch in device_switches:
            state_key = (switch.device_id, switch.key)

            # Turn on
            switch_futures[state_key] = loop.create_future()
            client.switch_command(switch.key, True, device_id=switch.device_id)
            try:
                await asyncio.wait_for(switch_futures[state_key], timeout=2.0)
            except TimeoutError:
                pytest.fail(
                    f"Timed out waiting for switch ON state "
                    f"(device_id={switch.device_id}, key={switch.key}). "
                    f"This likely means the firmware failed to decode the "
                    f"5-byte varint device_id in SwitchCommandRequest."
                )
            assert states[state_key].state is True

            # Turn off
            switch_futures[state_key] = loop.create_future()
            client.switch_command(switch.key, False, device_id=switch.device_id)
            try:
                await asyncio.wait_for(switch_futures[state_key], timeout=2.0)
            except TimeoutError:
                pytest.fail(
                    f"Timed out waiting for switch OFF state "
                    f"(device_id={switch.device_id}, key={switch.key})"
                )
            assert states[state_key].state is False
