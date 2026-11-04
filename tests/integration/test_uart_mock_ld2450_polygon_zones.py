"""Integration test for LD2450 polygon zones with mock UART.

Tests:
1. Polygons start empty and their presence sensors start unknown
2. A polygon set from the API is published back in canonical form
3. Invalid polygon text is rejected and the previous polygon is published again
4. Presence turns on and off as targets move in and out of each polygon
5. A zone without a polygon keeps presence unknown, and clearing a polygon makes it unknown again
"""

from __future__ import annotations

import asyncio
import re

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

from esphome.components.ld2450.text import POLYGON_MAX_LENGTH, POLYGON_PATTERN

from .host_prefs import clear_host_prefs
from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "uart-mock-ld2450-zones"
LEFT_POLYGON = "-1000,800;0,800;0,1500;-1000,1500"
NEAR_POLYGON = "0,0;1000,0;1000,600;0,600"


@pytest.mark.asyncio
async def test_uart_mock_ld2450_polygon_zones(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2450 polygon zone configuration and presence detection."""
    # Polygons are saved, so a previous run would otherwise restore them
    clear_host_prefs(DEVICE_NAME)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        polygons = {
            name: require_entity(entities, f"{name}_zone_polygon", TextInfo)
            for name in ("left", "near", "unset")
        }
        presences = {
            name: require_entity(entities, f"{name}_zone_presence", BinarySensorInfo)
            for name in ("left", "near", "unset")
        }
        presence_names = {info.key: name for name, info in presences.items()}
        # None records presence going back to unknown
        presence_history: dict[str, list[bool | None]] = {
            name: [] for name in presences
        }

        def on_state(state: EntityState) -> None:
            if isinstance(state, BinarySensorState) and state.key in presence_names:
                presence_history[presence_names[state.key]].append(
                    None if state.missing_state else state.state
                )

        sub = await subscribe_after_initial_states(client, entities, on_state=on_state)
        expect = sub.waiter.expect

        for name, polygon in polygons.items():
            # Home Assistant validates input and the zone card finds zones with these
            assert polygon.pattern == POLYGON_PATTERN
            assert polygon.max_length == POLYGON_MAX_LENGTH
            assert sub.initial_states[polygon.key].state == ""
            assert sub.initial_states[presences[name].key].missing_state

        async def set_polygon(name: str, value: str, expected: str) -> None:
            key = polygons[name].key
            published = expect(lambda s: isinstance(s, TextState) and s.key == key)
            client.text_command(key, value)
            state = await published
            assert state.state == expected
            assert re.fullmatch(POLYGON_PATTERN, state.state)

        # Spaces and a trailing separator are accepted and removed
        await set_polygon(
            "left", f" {LEFT_POLYGON.replace(';', ' ; ')} ;", LEFT_POLYGON
        )
        await set_polygon("near", NEAR_POLYGON, NEAR_POLYGON)
        # Invalid text keeps the polygon in use
        await set_polygon("near", "0,0;1000,0", NEAR_POLYGON)
        await set_polygon("near", "junk", NEAR_POLYGON)

        # Zone presence follows presence_timeout; keep the scenario short
        timeout = require_entity(entities, "presence_timeout", NumberInfo)
        timeout_set = expect(
            lambda s: (
                isinstance(s, NumberState) and s.key == timeout.key and s.state == 1
            )
        )
        client.number_command(timeout.key, 1)
        await timeout_set

        left_cleared = expect(
            lambda s: (
                isinstance(s, BinarySensorState)
                and s.key == presences["left"].key
                and s.state is False
            )
        )
        near_cleared = expect(
            lambda s: (
                isinstance(s, BinarySensorState)
                and s.key == presences["near"].key
                and s.state is False
            )
        )

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        await asyncio.gather(left_cleared, near_cleared)

        # Frame 1: (-500, 1000) is in the left zone and (200, 500) is in the near zone.
        # Frame 2: (300, 400) is in the near zone only; the left zone clears 1 s after frame 1.
        # Frame 3 onwards: no targets; the near zone clears 1 s after frame 2.
        assert presence_history["left"] == [True, False]
        assert presence_history["near"] == [True, False]
        assert presence_history["unset"] == []

        left_unknown = expect(
            lambda s: (
                isinstance(s, BinarySensorState)
                and s.key == presences["left"].key
                and s.missing_state
            )
        )
        await set_polygon("left", "", "")
        await left_unknown
        assert presence_history["left"] == [True, False, None]
