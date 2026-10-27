"""Integration test for LD2450 with presence_timeout set to 0.

Tests:
1. With 0 there is no hold: polygon zone presence clears on the first frame without a target, and has_target
   clears after its 1 s settle filter
2. presence_timeout stays 0 across a restart
"""

from __future__ import annotations

import asyncio
import socket

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

from .conftest import run_binary_and_wait_for_port, wait_and_connect_api_client
from .host_prefs import clear_host_prefs
from .state_utils import require_entity, subscribe_after_initial_states
from .types import CompileFunction, ConfigWriter

DEVICE_NAME = "uart-mock-ld2450-timeout-zero"
ZONE_POLYGON = "-1000,800;0,800;0,1500;-1000,1500"
# The radar sends a frame every 100 ms. The settle filter on has_target is 1 s.
FRAME_S = 0.1
SETTLE_S = 1.0


@pytest.mark.asyncio
async def test_uart_mock_ld2450_presence_timeout_zero(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Test that presence_timeout 0 clears presence without a hold and survives a restart."""
    port, port_socket = reserved_tcp_port
    # presence_timeout and the polygon are saved, so a previous run would otherwise restore them
    clear_host_prefs(DEVICE_NAME)

    binary_path = await compile_esphome(await write_yaml_config(yaml_config))
    # Release the reserved port so the binary can bind to it
    port_socket.close()

    loop = asyncio.get_running_loop()

    # First run: set 0, then a target leaves
    async with (
        run_binary_and_wait_for_port(binary_path, "127.0.0.1", port),
        wait_and_connect_api_client(port=port) as client,
    ):
        entities, _ = await client.list_entities_services()
        polygon = require_entity(entities, "hold_zone_polygon", TextInfo)
        zone = require_entity(entities, "hold_zone_presence", BinarySensorInfo)
        has_target = require_entity(entities, "has_target", BinarySensorInfo)
        timeout = require_entity(entities, "presence_timeout", NumberInfo)

        # (time received, state) for each change
        zone_history: list[tuple[float, bool]] = []
        has_target_history: list[tuple[float, bool]] = []
        polygon_set = loop.create_future()
        timeout_set = loop.create_future()
        has_target_off = loop.create_future()
        # Every presence_timeout state published after the connect time state
        timeout_states: list[float] = []

        def on_state(state: EntityState) -> None:
            if isinstance(state, BinarySensorState) and state.key == zone.key:
                zone_history.append((loop.time(), state.state))
            elif isinstance(state, BinarySensorState) and state.key == has_target.key:
                has_target_history.append((loop.time(), state.state))
                if not state.state and not has_target_off.done():
                    has_target_off.set_result(None)
            elif (
                isinstance(state, TextState)
                and state.key == polygon.key
                and state.state == ZONE_POLYGON
                and not polygon_set.done()
            ):
                polygon_set.set_result(None)
            elif isinstance(state, NumberState) and state.key == timeout.key:
                timeout_states.append(state.state)
                if state.state == 0 and not timeout_set.done():
                    timeout_set.set_result(None)

        sub = await subscribe_after_initial_states(client, entities, on_state=on_state)
        assert sub.initial_states[timeout.key].state == 5

        client.text_command(polygon.key, ZONE_POLYGON)
        client.number_command(timeout.key, 0)
        await asyncio.wait_for(asyncio.gather(polygon_set, timeout_set), timeout=5.0)

        # The old code restored the saved 5 s as soon as the number read 0
        await asyncio.sleep(0.5)
        assert timeout_states == [0]

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)
        await asyncio.wait_for(has_target_off, timeout=5.0)

        # The zone is on for the frame with the target and off on the next, with no hold
        assert [state for _, state in zone_history] == [True, False]
        (zone_on, _), (zone_off, _) = zone_history
        assert zone_off - zone_on < 5 * FRAME_S, (
            f"zone presence held for {zone_off - zone_on:.2f} s with presence_timeout 0"
        )

        # has_target is on at the first frame and off once its settle filter has passed, not 5 s later
        assert [state for _, state in has_target_history] == [True, False]
        (target_on, _), (target_off, _) = has_target_history
        assert (
            SETTLE_S - 2 * FRAME_S <= target_off - target_on <= SETTLE_S + 5 * FRAME_S
        ), f"has_target went off after {target_off - target_on:.2f} s"
        assert zone_off < target_off

        # The number never moved off 0 during the scenario
        assert timeout_states == [0]

    # Second run: 0 is restored, not replaced by the default 5 s
    async with (
        run_binary_and_wait_for_port(binary_path, "127.0.0.1", port),
        wait_and_connect_api_client(port=port) as client,
    ):
        entities, _ = await client.list_entities_services()
        timeout = require_entity(entities, "presence_timeout", NumberInfo)
        sub = await subscribe_after_initial_states(client, entities)
        state = sub.initial_states[timeout.key]
        assert isinstance(state, NumberState)
        assert state.state == 0

    clear_host_prefs(DEVICE_NAME)
