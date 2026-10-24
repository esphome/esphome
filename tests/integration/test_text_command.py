"""Integration test for text command zero-copy optimization.

Tests that TextCommandRequest correctly handles the pointer_to_buffer
optimization for the state field, ensuring text values are properly
transmitted via the API.
"""

from __future__ import annotations

from aioesphomeapi import TextInfo, TextState
import pytest

from .state_utils import (
    assert_device_name,
    require_entity,
    subscribe_after_initial_states,
)
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_text_command(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test text command with various string values including edge cases."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can get device info
        await assert_device_name(client, "host-text-command-test")

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

        # Wait for all initial states to be received
        sub = await subscribe_after_initial_states(client, entities)

        # Verify initial states were received
        assert test_text.key in sub.initial_states
        initial_text_state = sub.initial_states[test_text.key]
        assert isinstance(initial_text_state, TextState)
        assert initial_text_state.state == "initial"

        async def send_and_wait(key: int, value: str) -> TextState:
            """Send a text command and return the entity's next state."""
            next_text = sub.waiter.next_state(key, TextState, timeout=2.0)
            client.text_command(key=key, state=value)
            return await next_text

        # Test 1: Simple text value
        state = await send_and_wait(test_text.key, "hello world")
        assert state.state == "hello world"

        # Test 2: Empty string (edge case for zero-copy)
        state = await send_and_wait(test_text.key, "")
        assert state.state == ""

        # Test 3: Single character
        state = await send_and_wait(test_text.key, "x")
        assert state.state == "x"

        # Test 4: String with special characters
        state = await send_and_wait(test_text.key, "hello\tworld\n!")
        assert state.state == "hello\tworld\n!"

        # Test 5: Unicode characters
        state = await send_and_wait(test_text.key, "hello 世界 🌍")
        assert state.state == "hello 世界 🌍"

        # Test 6: Long string (tests buffer handling)
        long_text = "a" * 200
        state = await send_and_wait(test_text_long.key, long_text)
        assert state.state == long_text
        assert len(state.state) == 200

        # Test 7: Password field (same mechanism, different mode)
        state = await send_and_wait(test_password.key, "newpassword123")
        assert state.state == "newpassword123"

        # Test 8: String with null bytes embedded (edge case)
        # Note: protobuf strings should handle this but it's good to verify
        state = await send_and_wait(test_text.key, "before\x00after")
        assert state.state == "before\x00after"

        # Test 9: Rapid successive commands (tests buffer reuse)
        for i in range(5):
            state = await send_and_wait(test_text.key, f"rapid_{i}")
            assert state.state == f"rapid_{i}"
