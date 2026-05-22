"""Integration test for template text restore_value persistence.

Tests that:
1. A template text with restore_value saves its value to preferences
2. The saved value persists across restarts (binary re-run)
3. Setting the same value again does not produce a spurious "too long" warning
"""

from __future__ import annotations

import asyncio
import socket
from typing import Any

from aioesphomeapi import TextInfo, TextState
import pytest

from .conftest import run_binary_and_wait_for_port, wait_and_connect_api_client
from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper, require_entity
from .types import CompileFunction, ConfigWriter

DEVICE_NAME = "host-template-text-save-test"


@pytest.mark.asyncio
async def test_template_text_save(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Test template text save/restore persistence and duplicate-save behavior."""
    port, port_socket = reserved_tcp_port

    # Clean up any stale preference file from previous runs
    clear_host_prefs(DEVICE_NAME)

    # Write and compile once
    config_path = await write_yaml_config(yaml_config)
    binary_path = await compile_esphome(config_path)

    # Release the reserved port so the binary can bind to it
    port_socket.close()

    # --- First run: set a value and verify no spurious warnings ---
    warning_lines: list[str] = []

    def capture_warnings(line: str) -> None:
        if "too long to save" in line.lower():
            warning_lines.append(line)

    async with (
        run_binary_and_wait_for_port(
            binary_path, "127.0.0.1", port, line_callback=capture_warnings
        ),
        wait_and_connect_api_client(port=port) as client,
    ):
        device_info = await client.device_info()
        assert device_info.name == DEVICE_NAME

        entities, _ = await client.list_entities_services()
        text_entity = require_entity(
            entities, "test_text_restore", TextInfo, "Test Text Restore"
        )

        # Set up state tracking
        loop = asyncio.get_running_loop()
        state_futures: dict[int, asyncio.Future[Any]] = {}

        def on_state(state: Any) -> None:
            if state.key in state_futures and not state_futures[state.key].done():
                state_futures[state.key].set_result(state)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        # Verify initial value from config
        initial = initial_state_helper.initial_states[text_entity.key]
        assert isinstance(initial, TextState)
        assert initial.state == "hello"

        async def wait_for_state(key: int, timeout: float = 2.0) -> Any:
            state_futures[key] = loop.create_future()
            try:
                return await asyncio.wait_for(state_futures[key], timeout)
            finally:
                state_futures.pop(key, None)

        # Set a new value that fits within max_length
        client.text_command(key=text_entity.key, state="world")
        state = await wait_for_state(text_entity.key)
        assert state.state == "world"

        # Set the same value again - should NOT produce "too long" warning
        client.text_command(key=text_entity.key, state="world")
        # Give time for the warning to appear (if any)
        await asyncio.sleep(0.5)

    # No warnings should have appeared
    assert warning_lines == [], (
        f"Unexpected 'too long to save' warning(s): {warning_lines}"
    )

    # --- Second run: verify the value was restored from preferences ---
    async with (
        run_binary_and_wait_for_port(binary_path, "127.0.0.1", port),
        wait_and_connect_api_client(port=port) as client,
    ):
        entities, _ = await client.list_entities_services()
        text_entity = require_entity(
            entities, "test_text_restore", TextInfo, "Test Text Restore"
        )

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(lambda s: None))
        await initial_state_helper.wait_for_initial_states()

        # The value should be "world" - restored from preferences
        restored = initial_state_helper.initial_states[text_entity.key]
        assert isinstance(restored, TextState)
        assert restored.state == "world", (
            f"Expected restored value 'world', got '{restored.state}'"
        )

    # Clean up preference file
    clear_host_prefs(DEVICE_NAME)
