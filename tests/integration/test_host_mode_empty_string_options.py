"""Integration test for protobuf encoding of empty string options in select entities."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, SelectInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_empty_string_options(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that select entities with empty string options are correctly encoded in protobuf messages.

    This tests the fix for the bug where the force parameter was not passed in encode_string,
    causing empty strings in repeated fields to be skipped during encoding but included in
    size calculation, leading to protobuf decoding errors.
    """
    # Write, compile and run the ESPHome device, then connect to API
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can get device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "host-empty-string-test"

        # Get list of entities - this will encode ListEntitiesSelectResponse messages
        # with empty string options that would trigger the bug
        entity_info, services = await client.list_entities_services()

        # Find our select entities
        select_entities = [e for e in entity_info if isinstance(e, SelectInfo)]
        assert len(select_entities) == 3, (
            f"Expected 3 select entities, got {len(select_entities)}"
        )

        # Verify each select entity by name and check their options
        selects_by_name = {e.name: e for e in select_entities}

        # Check "Select Empty First" - empty string at beginning
        assert "Select Empty First" in selects_by_name
        empty_first = selects_by_name["Select Empty First"]
        assert len(empty_first.options) == 4
        assert empty_first.options[0] == ""  # Empty string at beginning
        assert empty_first.options[1] == "Option A"
        assert empty_first.options[2] == "Option B"
        assert empty_first.options[3] == "Option C"

        # Check "Select Empty Middle" - empty string in middle
        assert "Select Empty Middle" in selects_by_name
        empty_middle = selects_by_name["Select Empty Middle"]
        assert len(empty_middle.options) == 5
        assert empty_middle.options[0] == "Option 1"
        assert empty_middle.options[1] == "Option 2"
        assert empty_middle.options[2] == ""  # Empty string in middle
        assert empty_middle.options[3] == "Option 3"
        assert empty_middle.options[4] == "Option 4"

        # Check "Select Empty Last" - empty string at end
        assert "Select Empty Last" in selects_by_name
        empty_last = selects_by_name["Select Empty Last"]
        assert len(empty_last.options) == 4
        assert empty_last.options[0] == "Choice X"
        assert empty_last.options[1] == "Choice Y"
        assert empty_last.options[2] == "Choice Z"
        assert empty_last.options[3] == ""  # Empty string at end

        # If we got here without protobuf decoding errors, the fix is working
        # The bug would have caused "Invalid protobuf message" errors with trailing bytes

        # Also verify we can interact with the select entities
        # Subscribe to state changes
        states: dict[int, EntityState] = {}
        state_change_future: asyncio.Future[None] = loop.create_future()

        def on_state(state: EntityState) -> None:
            """Track state changes."""
            states[state.key] = state
            # When we receive the state change for our select, resolve the future
            if state.key == empty_first.key and not state_change_future.done():
                state_change_future.set_result(None)

        client.subscribe_states(on_state)

        # Try setting a select to an empty string option
        # This further tests that empty strings are handled correctly
        client.select_command(empty_first.key, "")

        # Wait for state update with timeout
        try:
            await asyncio.wait_for(state_change_future, timeout=5.0)
        except asyncio.TimeoutError:
            pytest.fail(
                "Did not receive state update after setting select to empty string"
            )

        # Verify the state was set to empty string
        assert empty_first.key in states
        select_state = states[empty_first.key]
        assert hasattr(select_state, "state")
        assert select_state.state == ""

        # The test passes if no protobuf decoding errors occurred
        # With the bug, we would have gotten "Invalid protobuf message" errors
