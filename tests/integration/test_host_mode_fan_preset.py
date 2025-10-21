"""Integration test for fan preset mode behavior."""

from __future__ import annotations

import asyncio

from aioesphomeapi import FanInfo, FanState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_fan_preset(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test fan preset mode behavior according to Home Assistant guidelines."""
    # Write, compile and run the ESPHome device, then connect to API
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get all fan entities
        entities = await client.list_entities_services()
        fans: list[FanInfo] = []
        for entity_list in entities:
            fans.extend(entity for entity in entity_list if isinstance(entity, FanInfo))

        # Create a map of fan names to entity info
        fan_map = {fan.name: fan for fan in fans}

        # Verify we have our test fans
        assert "Test Fan with Presets" in fan_map
        assert "Test Fan Simple" in fan_map
        assert "Test Fan No Speed" in fan_map

        # Get fan with presets
        fan_presets = fan_map["Test Fan with Presets"]
        assert fan_presets.supports_speed is True
        assert fan_presets.supported_speed_count == 5
        assert fan_presets.supports_oscillation is True
        assert fan_presets.supports_direction is True
        assert set(fan_presets.supported_preset_modes) == {"Eco", "Sleep", "Turbo"}

        # Subscribe to states
        states: dict[int, FanState] = {}
        state_event = asyncio.Event()
        initial_states_received = set()

        def on_state(state: FanState) -> None:
            if isinstance(state, FanState):
                states[state.key] = state
                initial_states_received.add(state.key)
                state_event.set()

        client.subscribe_states(on_state)

        # Wait for initial states to be received for all fans
        expected_fan_keys = {fan.key for fan in fans}
        while initial_states_received != expected_fan_keys:
            state_event.clear()
            await asyncio.wait_for(state_event.wait(), timeout=2.0)

        # Test 1: Turn on fan without speed or preset - should set speed to 100%
        state_event.clear()
        client.fan_command(
            key=fan_presets.key,
            state=True,
        )
        await asyncio.wait_for(state_event.wait(), timeout=2.0)

        fan_state = states[fan_presets.key]
        assert fan_state.state is True
        assert fan_state.speed_level == 5  # Should be max speed (100%)
        assert fan_state.preset_mode == ""

        # Turn off
        state_event.clear()
        client.fan_command(
            key=fan_presets.key,
            state=False,
        )
        await asyncio.wait_for(state_event.wait(), timeout=2.0)

        # Test 2: Turn on fan with preset mode - should NOT set speed to 100%
        state_event.clear()
        client.fan_command(
            key=fan_presets.key,
            state=True,
            preset_mode="Eco",
        )
        await asyncio.wait_for(state_event.wait(), timeout=2.0)

        fan_state = states[fan_presets.key]
        assert fan_state.state is True
        assert fan_state.preset_mode == "Eco"
        # Speed should be whatever the preset sets, not forced to 100%

        # Test 3: Setting speed should clear preset mode
        state_event.clear()
        client.fan_command(
            key=fan_presets.key,
            speed_level=3,
        )
        await asyncio.wait_for(state_event.wait(), timeout=2.0)

        fan_state = states[fan_presets.key]
        assert fan_state.state is True
        assert fan_state.speed_level == 3
        assert fan_state.preset_mode == ""  # Preset mode should be cleared

        # Test 4: Setting preset mode should work when fan is already on
        state_event.clear()
        client.fan_command(
            key=fan_presets.key,
            preset_mode="Sleep",
        )
        await asyncio.wait_for(state_event.wait(), timeout=2.0)

        fan_state = states[fan_presets.key]
        assert fan_state.state is True
        assert fan_state.preset_mode == "Sleep"

        # Turn off
        state_event.clear()
        client.fan_command(
            key=fan_presets.key,
            state=False,
        )
        await asyncio.wait_for(state_event.wait(), timeout=2.0)

        # Test 5: Turn on fan with specific speed
        state_event.clear()
        client.fan_command(
            key=fan_presets.key,
            state=True,
            speed_level=2,
        )
        await asyncio.wait_for(state_event.wait(), timeout=2.0)

        fan_state = states[fan_presets.key]
        assert fan_state.state is True
        assert fan_state.speed_level == 2
        assert fan_state.preset_mode == ""

        # Test 6: Test fan with no speed support
        fan_no_speed = fan_map["Test Fan No Speed"]
        assert fan_no_speed.supports_speed is False

        state_event.clear()
        client.fan_command(
            key=fan_no_speed.key,
            state=True,
        )
        await asyncio.wait_for(state_event.wait(), timeout=2.0)

        fan_state = states[fan_no_speed.key]
        assert fan_state.state is True
        # No speed should be set for fans that don't support speed
