"""Integration test for LD2450 polygon zone presence with presence_timeout.

Tests:
1. A target leaving the zone for a single radar frame does not clear presence
2. Presence clears once presence_timeout has passed since a target was last inside the zone
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import (
    BinarySensorInfo,
    BinarySensorState,
    ButtonInfo,
    EntityState,
    NumberInfo,
    NumberState,
    TextInfo,
    TextState,
)
import pytest

from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "uart-mock-ld2450-zone-timeout"
ZONE_POLYGON = "-1000,800;0,800;0,1500;-1000,1500"
PRESENCE_TIMEOUT_S = 1


@pytest.mark.asyncio
async def test_uart_mock_ld2450_polygon_zone_timeout(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that polygon zone presence follows presence_timeout."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )
    # The polygon and presence_timeout are saved, so a previous run would otherwise restore them
    clear_host_prefs(DEVICE_NAME)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        polygon = require_entity(entities, "hold_zone_polygon", TextInfo)
        presence = require_entity(entities, "hold_zone_presence", BinarySensorInfo)
        timeout = require_entity(entities, "presence_timeout", NumberInfo)

        loop = asyncio.get_running_loop()
        # (time received, state) for each presence change
        presence_history: list[tuple[float, bool]] = []
        cleared = loop.create_future()
        polygon_set = loop.create_future()
        timeout_set = loop.create_future()

        def on_state(state: EntityState) -> None:
            if isinstance(state, BinarySensorState) and state.key == presence.key:
                presence_history.append((loop.time(), state.state))
                if state.state is False and not cleared.done():
                    cleared.set_result(None)
            elif (
                isinstance(state, TextState)
                and state.key == polygon.key
                and state.state == ZONE_POLYGON
                and not polygon_set.done()
            ):
                polygon_set.set_result(None)
            elif (
                isinstance(state, NumberState)
                and state.key == timeout.key
                and state.state == PRESENCE_TIMEOUT_S
                and not timeout_set.done()
            ):
                timeout_set.set_result(None)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        client.text_command(polygon.key, ZONE_POLYGON)
        client.number_command(timeout.key, PRESENCE_TIMEOUT_S)
        await asyncio.wait_for(asyncio.gather(polygon_set, timeout_set), timeout=5.0)

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)
        await asyncio.wait_for(cleared, timeout=5.0)

        # The one-frame blip outside never reaches HA
        assert [state for _, state in presence_history] == [True, False]
        # The last inside frame comes 200 ms after the first, then presence_timeout must pass
        (on_time, _), (off_time, _) = presence_history
        assert off_time - on_time >= PRESENCE_TIMEOUT_S
