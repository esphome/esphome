"""Test multi-device preference storage functionality."""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import ButtonInfo, NumberInfo, SelectInfo, SwitchInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_multi_device_preferences(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that entities with same names on different devices have unique preference storage."""
    loop = asyncio.get_running_loop()
    log_lines: list[str] = []
    preferences_logged = loop.create_future()

    # Patterns to match preference hash logs
    switch_hash_pattern_device = re.compile(r"Device ([AB]) Switch Pref Hash: (\d+)")
    switch_hash_pattern_main = re.compile(r"Main Switch Pref Hash: (\d+)")
    number_hash_pattern_device = re.compile(r"Device ([AB]) Number Pref Hash: (\d+)")
    number_hash_pattern_main = re.compile(r"Main Number Pref Hash: (\d+)")
    switch_hashes: dict[str, int] = {}
    number_hashes: dict[str, int] = {}

    def check_output(line: str) -> None:
        """Check log output for preference hash information."""
        log_lines.append(line)

        # Look for device switch preference hash logs
        match = switch_hash_pattern_device.search(line)
        if match:
            device = match.group(1)
            hash_value = int(match.group(2))
            switch_hashes[device] = hash_value

        # Look for main switch preference hash
        match = switch_hash_pattern_main.search(line)
        if match:
            hash_value = int(match.group(1))
            switch_hashes["Main"] = hash_value

        # Look for device number preference hash logs
        match = number_hash_pattern_device.search(line)
        if match:
            device = match.group(1)
            hash_value = int(match.group(2))
            number_hashes[device] = hash_value

        # Look for main number preference hash
        match = number_hash_pattern_main.search(line)
        if match:
            hash_value = int(match.group(1))
            number_hashes["Main"] = hash_value

        # If we have all hashes, complete the future
        if (
            len(switch_hashes) == 3
            and len(number_hashes) == 3
            and not preferences_logged.done()
        ):
            preferences_logged.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Get entity list
        entities, _ = await client.list_entities_services()

        # Verify we have the expected entities with duplicate names on different devices

        # Check switches (3 with name "Light")
        switches = [
            e for e in entities if isinstance(e, SwitchInfo) and e.name == "Light"
        ]
        assert len(switches) == 3, f"Expected 3 'Light' switches, got {len(switches)}"

        # Check numbers (3 with name "Setpoint")
        numbers = [
            e for e in entities if isinstance(e, NumberInfo) and e.name == "Setpoint"
        ]
        assert len(numbers) == 3, f"Expected 3 'Setpoint' numbers, got {len(numbers)}"

        # Check selects (3 with name "Mode")
        selects = [
            e for e in entities if isinstance(e, SelectInfo) and e.name == "Mode"
        ]
        assert len(selects) == 3, f"Expected 3 'Mode' selects, got {len(selects)}"

        # Find the test button entity to trigger preference logging
        buttons = [e for e in entities if isinstance(e, ButtonInfo)]
        test_button = next((b for b in buttons if b.name == "Test Preferences"), None)
        assert test_button is not None, "Test Preferences button not found"

        # Press the button to trigger logging
        client.button_command(test_button.key)

        # Wait for preference hashes to be logged
        try:
            await asyncio.wait_for(preferences_logged, timeout=5.0)
        except TimeoutError:
            pytest.fail("Preference hashes not logged within timeout")

        # Verify all switch preference hashes are unique
        assert len(switch_hashes) == 3, (
            f"Expected 3 devices with switches, got {switch_hashes}"
        )
        switch_hash_values = list(switch_hashes.values())
        assert len(switch_hash_values) == len(set(switch_hash_values)), (
            f"Switch preference hashes are not unique: {switch_hashes}"
        )

        # Verify all number preference hashes are unique
        assert len(number_hashes) == 3, (
            f"Expected 3 devices with numbers, got {number_hashes}"
        )
        number_hash_values = list(number_hashes.values())
        assert len(number_hash_values) == len(set(number_hash_values)), (
            f"Number preference hashes are not unique: {number_hashes}"
        )

        # Verify Device A and Device B have different hashes (they have device_id set)
        assert switch_hashes["A"] != switch_hashes["B"], (
            f"Device A and B switches should have different hashes: A={switch_hashes['A']}, B={switch_hashes['B']}"
        )
        assert number_hashes["A"] != number_hashes["B"], (
            f"Device A and B numbers should have different hashes: A={number_hashes['A']}, B={number_hashes['B']}"
        )

        # Verify Main device hash is different from both A and B
        assert switch_hashes["Main"] != switch_hashes["A"], (
            f"Main and Device A switches should have different hashes: Main={switch_hashes['Main']}, A={switch_hashes['A']}"
        )
        assert switch_hashes["Main"] != switch_hashes["B"], (
            f"Main and Device B switches should have different hashes: Main={switch_hashes['Main']}, B={switch_hashes['B']}"
        )
