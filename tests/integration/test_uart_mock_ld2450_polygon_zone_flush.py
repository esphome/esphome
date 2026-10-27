"""Integration test for LD2450 polygon zones being flushed to flash on every edit.

The host build uses the default 60 s flash_write_interval, so a polygon that is in the
preferences file while the device runs was written by the edit itself.

Tests:
1. An accepted polygon is in the preferences file once Home Assistant shows it
2. An invalid polygon leaves the preferences file unchanged and the previous polygon is echoed
3. An empty polygon is saved with count 0, and the zone's presence goes back to unknown
"""

from __future__ import annotations

import struct

from aioesphomeapi import (
    BinarySensorInfo,
    BinarySensorState,
    ButtonInfo,
    TextInfo,
    TextState,
)
import pytest

from .host_prefs import clear_host_prefs, read_host_prefs
from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "uart-mock-ld2450-zone-flush"
ZONE_POLYGON = "-1000,800;0,800;0,1500;-1000,1500"
# The saved Polygon starts with its points as little-endian int16 x, y pairs
ZONE_POINTS = struct.pack("<8h", -1000, 800, 0, 800, 0, 1500, -1000, 1500)
# Offset of Polygon::count: after MAX_POLYGON_POINTS (23) points of 4 bytes
COUNT_OFFSET = 23 * 4


def saved_polygons() -> list[bytes]:
    """Return the saved records that are polygons: points, then the count."""
    return [
        data
        for data in read_host_prefs(DEVICE_NAME).values()
        if len(data) > COUNT_OFFSET
    ]


@pytest.mark.asyncio
async def test_uart_mock_ld2450_polygon_zone_flush(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that a polygon edit reaches the preferences file without waiting for the flash write interval."""
    clear_host_prefs(DEVICE_NAME)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        polygon = require_entity(entities, "flush_zone_polygon", TextInfo)
        presence = require_entity(entities, "flush_zone_presence", BinarySensorInfo)
        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        sub = await subscribe_after_initial_states(client, entities)
        expect = sub.waiter.expect

        assert sub.initial_states[polygon.key].state == ""
        assert sub.initial_states[presence.key].missing_state
        assert saved_polygons() == []

        async def set_polygon(value: str, expected: str) -> None:
            published = expect(
                lambda s: isinstance(s, TextState) and s.key == polygon.key
            )
            client.text_command(polygon.key, value)
            state = await published
            assert state.state == expected

        # The polygon is in the file as soon as it is echoed, with the device still running
        await set_polygon(ZONE_POLYGON, ZONE_POLYGON)
        records = saved_polygons()
        assert len(records) == 1, f"saved polygon not found in {records}"
        saved = records[0]
        assert saved.startswith(ZONE_POINTS)
        assert saved[COUNT_OFFSET] == 4

        # Invalid text changes nothing: the file keeps the polygon and the previous one is echoed
        await set_polygon("0,0;1000,0", ZONE_POLYGON)
        await set_polygon("junk", ZONE_POLYGON)
        assert saved_polygons() == [saved]

        # Put a target inside the polygon, so presence is known before the polygon is cleared
        present = expect(
            lambda s: (
                isinstance(s, BinarySensorState)
                and s.key == presence.key
                and s.state is True
            )
        )
        client.button_command(start_btn.key)
        await present

        # An empty polygon is saved too, and the zone's presence goes back to unknown
        unknown = expect(
            lambda s: (
                isinstance(s, BinarySensorState)
                and s.key == presence.key
                and s.missing_state
            )
        )
        await set_polygon("", "")
        await unknown
        records = saved_polygons()
        assert len(records) == 1
        assert records[0][COUNT_OFFSET] == 0

    clear_host_prefs(DEVICE_NAME)
