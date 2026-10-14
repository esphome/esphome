"""Test host preferences save and load functionality."""

from __future__ import annotations

import asyncio
import re
from typing import Any

from aioesphomeapi import ButtonInfo, EntityInfo, NumberInfo, SwitchInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


def find_entity_by_name(
    entities: list[EntityInfo], entity_type: type, name: str
) -> Any:
    """Helper to find an entity by type and name."""
    return next(
        (e for e in entities if isinstance(e, entity_type) and e.name == name), None
    )


@pytest.mark.asyncio
async def test_host_preferences_save_load(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that preferences are correctly saved and loaded after our optimization fix."""
    loop = asyncio.get_running_loop()
    log_lines: list[str] = []
    preferences_saved = loop.create_future()
    preferences_loaded = loop.create_future()
    values_match = loop.create_future()
    final_load_complete = loop.create_future()

    # Patterns to match preference logs
    save_pattern = re.compile(r"Preference saved: key=(\w+), value=([0-9.]+)")
    load_pattern = re.compile(r"Preference loaded: key=(\w+), value=([0-9.]+)")
    verify_pattern = re.compile(r"Preferences verified: values match!")
    final_load_success_pattern = re.compile(
        r"Final load test: loaded \d+ preferences successfully"
    )

    saved_values: dict[str, float] = {}
    loaded_values: dict[str, float] = {}

    def check_output(line: str) -> None:
        """Check log output for preference operations."""
        log_lines.append(line)

        # Look for save operations
        match = save_pattern.search(line)
        if match:
            key = match.group(1)
            value = float(match.group(2))
            saved_values[key] = value
            if len(saved_values) >= 2 and not preferences_saved.done():
                preferences_saved.set_result(True)

        # Look for load operations
        match = load_pattern.search(line)
        if match:
            key = match.group(1)
            value = float(match.group(2))
            loaded_values[key] = value
            if len(loaded_values) >= 2 and not preferences_loaded.done():
                preferences_loaded.set_result(True)

        # Look for verification
        if verify_pattern.search(line) and not values_match.done():
            values_match.set_result(True)

        # Look for final load test completion
        if final_load_success_pattern.search(line) and not final_load_complete.done():
            final_load_complete.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Get entity list
        entities, _ = await client.list_entities_services()

        # Find our test entities using helper
        test_switch = find_entity_by_name(entities, SwitchInfo, "Test Switch")
        test_number = find_entity_by_name(entities, NumberInfo, "Test Number")
        save_button = find_entity_by_name(entities, ButtonInfo, "Save Preferences")
        load_button = find_entity_by_name(entities, ButtonInfo, "Load Preferences")
        verify_button = find_entity_by_name(entities, ButtonInfo, "Verify Preferences")

        assert test_switch is not None, "Test Switch not found"
        assert test_number is not None, "Test Number not found"
        assert save_button is not None, "Save Preferences button not found"
        assert load_button is not None, "Load Preferences button not found"
        assert verify_button is not None, "Verify Preferences button not found"

        # Set initial values
        client.switch_command(test_switch.key, True)
        client.number_command(test_number.key, 42.5)

        # Save preferences
        client.button_command(save_button.key)

        # Wait for save to complete
        try:
            await asyncio.wait_for(preferences_saved, timeout=5.0)
        except TimeoutError:
            pytest.fail("Preferences not saved within timeout")

        # Verify we saved the expected values
        assert "switch" in saved_values, f"Switch preference not saved: {saved_values}"
        assert "number" in saved_values, f"Number preference not saved: {saved_values}"
        assert saved_values["switch"] == 1.0, (
            f"Switch value incorrect: {saved_values['switch']}"
        )
        assert saved_values["number"] == 42.5, (
            f"Number value incorrect: {saved_values['number']}"
        )

        # Change the values to something else
        client.switch_command(test_switch.key, False)
        client.number_command(test_number.key, 13.7)

        # Load preferences (should restore the saved values)
        client.button_command(load_button.key)

        # Wait for load to complete
        try:
            await asyncio.wait_for(preferences_loaded, timeout=5.0)
        except TimeoutError:
            pytest.fail("Preferences not loaded within timeout")

        # Verify loaded values match saved values
        assert "switch" in loaded_values, (
            f"Switch preference not loaded: {loaded_values}"
        )
        assert "number" in loaded_values, (
            f"Number preference not loaded: {loaded_values}"
        )
        assert loaded_values["switch"] == saved_values["switch"], (
            f"Loaded switch value {loaded_values['switch']} doesn't match saved {saved_values['switch']}"
        )
        assert loaded_values["number"] == saved_values["number"], (
            f"Loaded number value {loaded_values['number']} doesn't match saved {saved_values['number']}"
        )

        # Verify the values were actually restored
        client.button_command(verify_button.key)

        # Wait for verification
        try:
            await asyncio.wait_for(values_match, timeout=5.0)
        except TimeoutError:
            pytest.fail("Preference verification failed within timeout")

        # Test that non-existent preferences don't crash (tests our fix)
        # This will trigger load attempts for keys that don't exist
        # Our fix should prevent map entries from being created
        client.button_command(load_button.key)

        # Wait for the final load test to complete
        try:
            await asyncio.wait_for(final_load_complete, timeout=5.0)
        except TimeoutError:
            pytest.fail("Final load test did not complete within timeout")
