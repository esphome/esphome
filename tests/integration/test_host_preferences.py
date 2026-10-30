"""Test host preferences save and load functionality."""

from __future__ import annotations

import re

from aioesphomeapi import ButtonInfo, NumberInfo, SwitchInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

VALUE_PATTERN = re.compile(r"value=([0-9.]+)")


@pytest.mark.asyncio
async def test_host_preferences_save_load(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that preferences are correctly saved and loaded after our optimization fix."""
    waiter = LineWaiter()

    async def logged_value(action: str, key: str) -> float:
        line = await waiter.wait_for(f"Preference {action}: key={key},", timeout=5.0)
        match = VALUE_PATTERN.search(line)
        assert match is not None, f"Unexpected preference line: {line}"
        return float(match.group(1))

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Get entity list
        entities, _ = await client.list_entities_services()

        test_switch = require_entity(entities, "test_switch", SwitchInfo, exact=True)
        test_number = require_entity(entities, "test_number", NumberInfo, exact=True)
        save_button = require_entity(
            entities, "save_preferences", ButtonInfo, exact=True
        )
        load_button = require_entity(
            entities, "load_preferences", ButtonInfo, exact=True
        )
        verify_button = require_entity(
            entities, "verify_preferences", ButtonInfo, exact=True
        )

        # Set initial values
        client.switch_command(test_switch.key, True)
        client.number_command(test_number.key, 42.5)

        # Save preferences
        client.button_command(save_button.key)

        saved_switch = await logged_value("saved", "switch")
        saved_number = await logged_value("saved", "number")
        assert saved_switch == 1.0, f"Switch value incorrect: {saved_switch}"
        assert saved_number == 42.5, f"Number value incorrect: {saved_number}"

        # Change the values to something else
        client.switch_command(test_switch.key, False)
        client.number_command(test_number.key, 13.7)

        # Load preferences (should restore the saved values)
        client.button_command(load_button.key)

        loaded_switch = await logged_value("loaded", "switch")
        loaded_number = await logged_value("loaded", "number")
        assert loaded_switch == saved_switch, (
            f"Loaded switch value {loaded_switch} doesn't match saved {saved_switch}"
        )
        assert loaded_number == saved_number, (
            f"Loaded number value {loaded_number} doesn't match saved {saved_number}"
        )

        # Verify the values were actually restored
        client.button_command(verify_button.key)

        # Wait for verification
        await waiter.wait_for("Preferences verified: values match!", timeout=5.0)

        # Test that non-existent preferences don't crash (tests our fix)
        # This will trigger load attempts for keys that don't exist
        # Our fix should prevent map entries from being created
        client.button_command(load_button.key)

        # Wait for the final load test to complete
        await waiter.wait_for(
            "Final load test: loaded", "preferences successfully", timeout=5.0
        )
