"""Integration test for restoring LD2450 polygon zones from saved preferences.

Tests:
1. A polygon set from the API is restored after a restart
2. A saved polygon that is no longer valid is reported and leaves the zone disabled
"""

from __future__ import annotations

import asyncio
import socket
import struct

from aioesphomeapi import TextInfo, TextState
import pytest

from .conftest import run_binary_and_wait_for_port, wait_and_connect_api_client
from .host_prefs import clear_host_prefs, read_host_prefs, write_host_prefs
from .log_utils import LineWaiter
from .state_utils import InitialStateHelper, require_entity
from .types import CompileFunction, ConfigWriter

DEVICE_NAME = "uart-mock-ld2450-zone-restore"
ZONE_POLYGON = "-1000,800;0,800;0,1500;-1000,1500"
# The saved Polygon starts with its points as little-endian int16 x, y pairs
SAVED_POLYGON_PREFIX = struct.pack("<hh", -1000, 800)
# Offset of Polygon::count: after MAX_POLYGON_POINTS (23) points of 4 bytes
COUNT_OFFSET = 23 * 4


@pytest.mark.asyncio
async def test_uart_mock_ld2450_polygon_zone_restore(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Test that polygon zones are restored after a restart, and invalid saved polygons are rejected."""
    port, port_socket = reserved_tcp_port
    clear_host_prefs(DEVICE_NAME)

    binary_path = await compile_esphome(await write_yaml_config(yaml_config))
    # Release the reserved port so the binary can bind to it
    port_socket.close()

    async def initial_polygon(line_callback=None) -> str:
        """Start the binary and return the zone's polygon state as first published."""
        async with (
            run_binary_and_wait_for_port(
                binary_path, "127.0.0.1", port, line_callback=line_callback
            ),
            wait_and_connect_api_client(port=port) as client,
        ):
            entities, _ = await client.list_entities_services()
            polygon = require_entity(entities, "restore_zone_polygon", TextInfo)
            helper = InitialStateHelper(entities)
            client.subscribe_states(helper.on_state_wrapper(lambda state: None))
            await helper.wait_for_initial_states()
            state = helper.initial_states[polygon.key]
            assert isinstance(state, TextState)
            return state.state

    # First run: set a polygon, which is saved
    async with (
        run_binary_and_wait_for_port(binary_path, "127.0.0.1", port),
        wait_and_connect_api_client(port=port) as client,
    ):
        entities, _ = await client.list_entities_services()
        polygon = require_entity(entities, "restore_zone_polygon", TextInfo)
        loop = asyncio.get_running_loop()
        published = loop.create_future()

        def on_state(state: object) -> None:
            if (
                isinstance(state, TextState)
                and state.key == polygon.key
                and state.state == ZONE_POLYGON
                and not published.done()
            ):
                published.set_result(None)

        helper = InitialStateHelper(entities)
        client.subscribe_states(helper.on_state_wrapper(on_state))
        await helper.wait_for_initial_states()
        assert helper.initial_states[polygon.key].state == ""

        client.text_command(polygon.key, ZONE_POLYGON)
        await asyncio.wait_for(published, timeout=5.0)

    # Second run: the polygon is restored
    assert await initial_polygon() == ZONE_POLYGON

    # Third run: a saved polygon with too few points is rejected
    prefs = read_host_prefs(DEVICE_NAME)
    keys = [key for key, data in prefs.items() if data.startswith(SAVED_POLYGON_PREFIX)]
    assert len(keys) == 1, f"saved polygon not found in {prefs}"
    data = bytearray(prefs[keys[0]])
    data[COUNT_OFFSET] = 2
    prefs[keys[0]] = bytes(data)
    write_host_prefs(DEVICE_NAME, prefs)

    waiter = LineWaiter()
    assert await initial_polygon(waiter.callback) == ""
    assert any("saved polygon is invalid" in line for line in waiter.lines), (
        "expected a warning about the invalid saved polygon"
    )

    clear_host_prefs(DEVICE_NAME)
