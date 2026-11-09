"""Integration test for TextSensor get_raw_state() and StringRef-based filters.

This tests:
1. The optimization in PR #12205 where raw_state is only stored when filters
   are configured. When no filters exist, get_raw_state() should return state.
2. StringRef-based filters (append, prepend, substitute, map) which store
   static string data in flash instead of heap-allocating std::string.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

STATE_PATTERN = re.compile(r"state='([^']*)'(?: raw_state='([^']*)')?")


@pytest.mark.asyncio
async def test_text_sensor_raw_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test text sensor filters and raw_state behavior.

    Tests:
    1. get_raw_state() without filters returns same as state
    2. get_raw_state() with filters returns original (unfiltered) value
    3. StringRef-based filters: append, prepend, substitute, map, chained
    """
    waiter = LineWaiter()

    async def logged_state(label: str) -> tuple[str, str | None]:
        line = await waiter.wait_for(f"{label}: state='", timeout=5.0)
        match = STATE_PATTERN.search(line)
        assert match is not None, f"Unexpected {label} line: {line}"
        return match.group(1), match.group(2)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "test-text-sensor-raw-state")

        # Get entities to find our buttons
        entities, _ = await client.list_entities_services()

        # Find the test buttons
        no_filter_button = require_entity(entities, "test_no_filter_button")

        with_filter_button = require_entity(entities, "test_with_filter_button")

        # Test 1: Text sensor without filters
        # get_raw_state() should return the same as state
        client.button_command(no_filter_button.key)

        state, raw_state = await logged_state("NO_FILTER")

        assert state == "hello world", f"Expected state='hello world', got '{state}'"
        assert raw_state == "hello world", (
            f"Expected raw_state='hello world', got '{raw_state}'"
        )
        assert state == raw_state, (
            f"Without filters, state and raw_state should be equal. "
            f"state='{state}', raw_state='{raw_state}'"
        )

        # Test 2: Text sensor with to_upper filter
        # state should be filtered (uppercase), raw_state should be original
        client.button_command(with_filter_button.key)

        state, raw_state = await logged_state("WITH_FILTER")

        assert state == "HELLO WORLD", f"Expected state='HELLO WORLD', got '{state}'"
        assert raw_state == "hello world", (
            f"Expected raw_state='hello world', got '{raw_state}'"
        )
        assert state != raw_state, (
            f"With filters, state and raw_state should differ. "
            f"state='{state}', raw_state='{raw_state}'"
        )

        # Test 3: Append filter (StringRef-based)
        # "test" + " suffix" = "test suffix"
        append_button = require_entity(entities, "test_append_button")
        client.button_command(append_button.key)

        state, _ = await logged_state("APPEND")

        assert state == "test suffix", (
            f"Append failed: expected 'test suffix', got '{state}'"
        )

        # Test 4: Prepend filter (StringRef-based)
        # "prefix " + "test" = "prefix test"
        prepend_button = require_entity(entities, "test_prepend_button")
        client.button_command(prepend_button.key)

        state, _ = await logged_state("PREPEND")

        assert state == "prefix test", (
            f"Prepend failed: expected 'prefix test', got '{state}'"
        )

        # Test 5: Substitute filter (StringRef-based)
        # "foo says hello" with foo->bar, hello->world = "bar says world"
        substitute_button = require_entity(entities, "test_substitute_button")
        client.button_command(substitute_button.key)

        state, _ = await logged_state("SUBSTITUTE")

        assert state == "bar says world", (
            f"Substitute failed: expected 'bar says world', got '{state}'"
        )

        # Test 6: Map filter - "ON" -> "Active"
        map_on_button = require_entity(entities, "test_map_on_button")
        client.button_command(map_on_button.key)

        state, _ = await logged_state("MAP_ON")

        assert state == "Active", f"Map ON failed: expected 'Active', got '{state}'"

        # Test 7: Map filter - "OFF" -> "Inactive"
        map_off_button = require_entity(entities, "test_map_off_button")
        client.button_command(map_off_button.key)

        state, _ = await logged_state("MAP_OFF")

        assert state == "Inactive", (
            f"Map OFF failed: expected 'Inactive', got '{state}'"
        )

        # Test 8: Map filter - passthrough for unknown values
        # "UNKNOWN" -> "UNKNOWN" (no match, passes through unchanged)
        map_unknown_button = require_entity(entities, "test_map_unknown_button")
        client.button_command(map_unknown_button.key)

        state, _ = await logged_state("MAP_UNKNOWN")

        assert state == "UNKNOWN", (
            f"Map passthrough failed: expected 'UNKNOWN', got '{state}'"
        )

        # Test 9: Chained filters (prepend "[" + append "]")
        # "[" + "value" + "]" = "[value]"
        chained_button = require_entity(entities, "test_chained_button")
        client.button_command(chained_button.key)

        state, _ = await logged_state("CHAINED")

        assert state == "[value]", f"Chained failed: expected '[value]', got '{state}'"

        # Test 10: to_lower filter
        # "HELLO WORLD" -> "hello world"
        to_lower_button = require_entity(entities, "test_to_lower_button")
        client.button_command(to_lower_button.key)

        state, _ = await logged_state("TO_LOWER")

        assert state == "hello world", (
            f"to_lower failed: expected 'hello world', got '{state}'"
        )

        # Test 11: Lambda filter
        # "test" -> "[test]"
        lambda_button = require_entity(entities, "test_lambda_button")
        client.button_command(lambda_button.key)

        state, _ = await logged_state("LAMBDA")

        assert state == "[test]", f"Lambda failed: expected '[test]', got '{state}'"

        # Test 12: Lambda filter - value passes through
        # "value" -> "value passed"
        lambda_pass_button = require_entity(entities, "test_lambda_pass_button")
        client.button_command(lambda_pass_button.key)

        state, _ = await logged_state("LAMBDA_PASS")

        assert state == "value passed", (
            f"Lambda pass failed: expected 'value passed', got '{state}'"
        )

        # Test 13: Lambda filter - skip publishing (return {})
        # "skip" -> no publish, state remains "value passed" from previous test
        lambda_skip_button = require_entity(entities, "test_lambda_skip_button")
        client.button_command(lambda_skip_button.key)

        state, _ = await logged_state("LAMBDA_SKIP")

        # When lambda returns {}, value should NOT be published
        # State remains from previous successful publish ("value passed")
        assert state == "value passed", (
            f"Lambda skip failed: expected 'value passed' (unchanged), got '{state}'"
        )

        # Test 14: Lambda filter - verify raw_state is preserved (not mutated)
        # This is critical to verify the in-place mutation optimization is safe
        # "original" -> state="original MODIFIED", raw_state="original"
        lambda_raw_state_button = require_entity(
            entities, "test_lambda_raw_state_button"
        )
        client.button_command(lambda_raw_state_button.key)

        state, raw_state = await logged_state("LAMBDA_RAW_STATE")

        assert state == "original MODIFIED", (
            f"Lambda raw_state test failed: expected state='original MODIFIED', "
            f"got '{state}'"
        )
        assert raw_state == "original", (
            f"Lambda raw_state test failed: raw_state was mutated! "
            f"Expected 'original', got '{raw_state}'"
        )
        assert state != raw_state, (
            f"Lambda filter should modify state but preserve raw_state. "
            f"state='{state}', raw_state='{raw_state}'"
        )
