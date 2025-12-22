"""Integration test for text command zero-copy optimization.

Tests that TextCommandRequest correctly handles the pointer_to_buffer
optimization for the state field, ensuring text values are properly
transmitted via the API.
"""

from __future__ import annotations

import asyncio
from typing import Any

from aioesphomeapi import TextInfo, TextState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_text_command(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test text command with various string values including edge cases."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can get device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "host-text-command-test"

        # Get list of entities
        entities, _ = await client.list_entities_services()

        # Find our text entities using require_entity
        test_text = require_entity(entities, "test_text", TextInfo, "Test Text entity")
        test_password = require_entity(
            entities, "test_password", TextInfo, "Test Password entity"
        )
        test_text_long = require_entity(
            entities, "test_text_long", TextInfo, "Test Text Long entity"
        )

        # Track state changes
        states: dict[int, Any] = {}
        state_futures: dict[int, asyncio.Future[Any]] = {}

        def on_state(state: Any) -> None:
            states[state.key] = state
            if state.key in state_futures and not state_futures[state.key].done():
                state_futures[state.key].set_result(state)

        # Set up InitialStateHelper to swallow initial state broadcasts
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for all initial states to be received
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Verify initial states were received
        assert test_text.key in initial_state_helper.initial_states
        initial_text_state = initial_state_helper.initial_states[test_text.key]
        assert isinstance(initial_text_state, TextState)
        assert initial_text_state.state == "initial"

        async def wait_for_state_change(key: int, timeout: float = 2.0) -> Any:
            """Wait for a state change for the given entity key."""
            state_futures[key] = loop.create_future()
            try:
                return await asyncio.wait_for(state_futures[key], timeout)
            finally:
                state_futures.pop(key, None)

        # Test 1: Simple text value
        client.text_command(key=test_text.key, state="hello world")
        state = await wait_for_state_change(test_text.key)
        assert state.state == "hello world"

        # Test 2: Empty string (edge case for zero-copy)
        client.text_command(key=test_text.key, state="")
        state = await wait_for_state_change(test_text.key)
        assert state.state == ""

        # Test 3: Single character
        client.text_command(key=test_text.key, state="x")
        state = await wait_for_state_change(test_text.key)
        assert state.state == "x"

        # Test 4: String with special characters
        client.text_command(key=test_text.key, state="hello\tworld\n!")
        state = await wait_for_state_change(test_text.key)
        assert state.state == "hello\tworld\n!"

        # Test 5: Unicode characters
        client.text_command(key=test_text.key, state="hello 世界 🌍")
        state = await wait_for_state_change(test_text.key)
        assert state.state == "hello 世界 🌍"

        # Test 6: Long string (tests buffer handling)
        long_text = "a" * 200
        client.text_command(key=test_text_long.key, state=long_text)
        state = await wait_for_state_change(test_text_long.key)
        assert state.state == long_text
        assert len(state.state) == 200

        # Test 7: Password field (same mechanism, different mode)
        client.text_command(key=test_password.key, state="newpassword123")
        state = await wait_for_state_change(test_password.key)
        assert state.state == "newpassword123"

        # Test 8: String with null bytes embedded (edge case)
        # Note: protobuf strings should handle this but it's good to verify
        client.text_command(key=test_text.key, state="before\x00after")
        state = await wait_for_state_change(test_text.key)
        assert state.state == "before\x00after"

        # Test 9: Rapid successive commands (tests buffer reuse)
        for i in range(5):
            client.text_command(key=test_text.key, state=f"rapid_{i}")
            state = await wait_for_state_change(test_text.key)
            assert state.state == f"rapid_{i}"
