"""Integration test for LD2450 polygon zones with mock UART.

Tests:
1. Polygons start empty and their presence sensors start off
2. A polygon set from the API is published back in canonical form
3. Invalid polygon text is rejected and the previous polygon is published again
4. Presence turns on and off as targets move in and out of each polygon
5. A zone without a polygon never reports presence
"""

from __future__ import annotations

import asyncio
from collections.abc import Callable
from pathlib import Path

from aioesphomeapi import (
    BinarySensorInfo,
    BinarySensorState,
    ButtonInfo,
    EntityState,
    TextInfo,
    TextState,
)
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

LEFT_POLYGON = "-1000,800;0,800;0,1500;-1000,1500"
NEAR_POLYGON = "0,0;1000,0;1000,600;0,600"


@pytest.mark.asyncio
async def test_uart_mock_ld2450_polygon_zones(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2450 polygon zone configuration and presence detection."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

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
        presence_history: dict[str, list[bool]] = {name: [] for name in presences}
        loop = asyncio.get_running_loop()
        waits: list[tuple[Callable[[EntityState], bool], asyncio.Future]] = []

        def expect(predicate: Callable[[EntityState], bool]) -> asyncio.Future:
            """Arm a wait for the next state matching predicate."""
            future = loop.create_future()
            waits.append((predicate, future))
            return future

        def on_state(state: EntityState) -> None:
            if isinstance(state, BinarySensorState) and state.key in presence_names:
                presence_history[presence_names[state.key]].append(state.state)
            for predicate, future in waits:
                if not future.done() and predicate(state):
                    future.set_result(state)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        for name in polygons:
            assert initial_state_helper.initial_states[polygons[name].key].state == ""
            assert (
                initial_state_helper.initial_states[presences[name].key].state is False
            )

        async def set_polygon(name: str, value: str, expected: str) -> None:
            key = polygons[name].key
            published = expect(lambda s: isinstance(s, TextState) and s.key == key)
            client.text_command(key, value)
            state = await asyncio.wait_for(published, timeout=5.0)
            assert state.state == expected

        # Spaces and a trailing separator are accepted and removed
        await set_polygon(
            "left", f" {LEFT_POLYGON.replace(';', ' ; ')} ;", LEFT_POLYGON
        )
        await set_polygon("near", NEAR_POLYGON, NEAR_POLYGON)
        # Invalid text keeps the polygon in use
        await set_polygon("near", "0,0;1000,0", NEAR_POLYGON)
        await set_polygon("near", "junk", NEAR_POLYGON)

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

        await asyncio.wait_for(asyncio.gather(left_cleared, near_cleared), timeout=5.0)

        # Frame 1: (-500, 1000) is in the left zone and (200, 500) is in the near zone.
        # Frame 2: (300, 400) is in the near zone only, so only the left zone clears.
        # Frame 3: no targets, so the near zone clears.
        assert presence_history["left"] == [True, False]
        assert presence_history["near"] == [True, False]
        assert presence_history["unset"] == []
