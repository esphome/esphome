"""Integration test for template text restore_value persistence.

Tests that:
1. A template text with restore_value saves its value to preferences
2. The saved value persists across restarts (binary re-run)
3. Setting the same value again does not produce a spurious "too long" warning
"""

from __future__ import annotations

import asyncio
import socket

from aioesphomeapi import TextInfo, TextState
import pytest

from .conftest import run_binary_and_wait_for_port, wait_and_connect_api_client
from .host_prefs import clear_host_prefs
from .log_utils import LineWaiter
from .state_utils import (
    assert_device_name,
    require_entity,
    subscribe_after_initial_states,
)
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
    waiter = LineWaiter()

    async with (
        run_binary_and_wait_for_port(
            binary_path, "127.0.0.1", port, line_callback=waiter.callback
        ),
        wait_and_connect_api_client(port=port) as client,
    ):
        await assert_device_name(client, DEVICE_NAME)

        entities, _ = await client.list_entities_services()
        text_entity = require_entity(
            entities, "test_text_restore", TextInfo, "Test Text Restore"
        )

        sub = await subscribe_after_initial_states(client, entities)

        # Verify initial value from config
        initial = sub.initial_states[text_entity.key]
        assert isinstance(initial, TextState)
        assert initial.state == "hello"

        # Set a new value that fits within max_length
        next_text = sub.waiter.next_state(text_entity.key, TextState, timeout=2.0)
        client.text_command(key=text_entity.key, state="world")
        state = await next_text
        assert state.state == "world"

        # Set the same value again - should NOT produce "too long" warning
        client.text_command(key=text_entity.key, state="world")
        # Give time for the warning to appear (if any)
        await asyncio.sleep(0.5)

    # No warnings should have appeared
    warning_lines = [
        line for line in waiter.lines if "too long to save" in line.lower()
    ]
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

        sub = await subscribe_after_initial_states(client, entities)

        # The value should be "world" - restored from preferences
        restored = sub.initial_states[text_entity.key]
        assert isinstance(restored, TextState)
        assert restored.state == "world", (
            f"Expected restored value 'world', got '{restored.state}'"
        )

    # Clean up preference file
    clear_host_prefs(DEVICE_NAME)
