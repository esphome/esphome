"""Integration test for the light `restore_state:` key.

Tests that:
1. On first boot (nothing saved yet), lights come up at their hardware default (off) --
   restore_state overrides never apply before anything is ever saved.
2. After a state is saved and the device restarts, a `restore_state: {}` light comes
   back exactly as saved, while a light with explicit overrides applies those overrides
   on top of the loaded state regardless of what was saved.
"""

from __future__ import annotations

import asyncio
import socket
from typing import Any

from aioesphomeapi import LightInfo, LightState
import pytest

from .conftest import run_binary_and_wait_for_port, wait_and_connect_api_client
from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper, require_entity
from .types import CompileFunction, ConfigWriter

DEVICE_NAME = "host-light-restore-state-test"


@pytest.mark.asyncio
async def test_light_restore_state(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Test restore_state: {} vs restore_state: with overrides, across a restart."""
    port, port_socket = reserved_tcp_port

    clear_host_prefs(DEVICE_NAME)

    config_path = await write_yaml_config(yaml_config)
    binary_path = await compile_esphome(config_path)

    port_socket.close()

    # --- First run: nothing saved yet, both lights must come up off ---
    async with (
        run_binary_and_wait_for_port(binary_path, "127.0.0.1", port),
        wait_and_connect_api_client(port=port) as client,
    ):
        entities, _ = await client.list_entities_services()
        keep_light = require_entity(entities, "test_light_keep", LightInfo)
        override_light = require_entity(entities, "test_light_override", LightInfo)

        # A single subscription serves both initial-state capture and later
        # state-change waits -- a second subscribe_states call would restart the
        # device's initial-state iterator and could resolve a wait on replayed data.
        loop = asyncio.get_running_loop()
        state_futures: dict[int, asyncio.Future[Any]] = {}

        def on_state(state: Any) -> None:
            if state.key in state_futures and not state_futures[state.key].done():
                state_futures[state.key].set_result(state)

        helper = InitialStateHelper(entities)
        client.subscribe_states(helper.on_state_wrapper(on_state))
        await helper.wait_for_initial_states()

        keep_initial = helper.initial_states[keep_light.key]
        assert isinstance(keep_initial, LightState)
        assert keep_initial.state is False

        override_initial = helper.initial_states[override_light.key]
        assert isinstance(override_initial, LightState)
        assert override_initial.state is False

        # Set both lights to a known, saved state
        async def wait_for_state(key: int, timeout: float = 2.0) -> Any:
            state_futures[key] = loop.create_future()
            try:
                return await asyncio.wait_for(state_futures[key], timeout)
            finally:
                state_futures.pop(key, None)

        client.light_command(key=keep_light.key, state=True, brightness=0.64)
        await wait_for_state(keep_light.key)

        client.light_command(key=override_light.key, state=True, brightness=0.3)
        await wait_for_state(override_light.key)

    # --- Second run: same binary, same prefs file ---
    async with (
        run_binary_and_wait_for_port(binary_path, "127.0.0.1", port),
        wait_and_connect_api_client(port=port) as client,
    ):
        entities, _ = await client.list_entities_services()
        keep_light = require_entity(entities, "test_light_keep", LightInfo)
        override_light = require_entity(entities, "test_light_override", LightInfo)

        helper = InitialStateHelper(entities)
        client.subscribe_states(helper.on_state_wrapper(lambda s: None))
        await helper.wait_for_initial_states()

        # restore_state: {} -- comes back exactly as saved
        keep_state = helper.initial_states[keep_light.key]
        assert isinstance(keep_state, LightState)
        assert keep_state.state is True
        assert keep_state.brightness == pytest.approx(0.64, abs=0.01)

        # restore_state: with overrides -- state inverted, brightness forced to 100%
        # regardless of what was actually saved (0.3)
        override_state = helper.initial_states[override_light.key]
        assert isinstance(override_state, LightState)
        assert override_state.state is False
        assert override_state.brightness == pytest.approx(1.0, abs=0.01)

    clear_host_prefs(DEVICE_NAME)
