"""Test multi-device preference storage functionality."""

from __future__ import annotations

import re

from aioesphomeapi import ButtonInfo, NumberInfo, SelectInfo, SwitchInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

# Keys are "A", "B" and "Main"
SWITCH_HASH_RE = re.compile(r"(?:Device )?\b([AB]|Main) Switch Pref Hash: (\d+)")
NUMBER_HASH_RE = re.compile(r"(?:Device )?\b([AB]|Main) Number Pref Hash: (\d+)")


@pytest.mark.asyncio
async def test_multi_device_preferences(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that entities with same names on different devices have unique preference storage."""
    waiter = LineWaiter()

    def hashes(pattern: re.Pattern[str]) -> dict[str, int]:
        return {
            match[1]: int(match[2])
            for line in waiter.lines
            if (match := pattern.search(line))
        }

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
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
        test_button = require_entity(
            entities, "test_preferences", ButtonInfo, exact=True
        )

        # Press the button to trigger logging
        client.button_command(test_button.key)

        # The last of the six hash lines
        await wait_or_fail(
            waiter.wait_for("Main Number Pref Hash:", timeout=20.0),
            5.0,
            "Preference hashes not logged within timeout",
        )
        switch_hashes = hashes(SWITCH_HASH_RE)
        number_hashes = hashes(NUMBER_HASH_RE)

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
