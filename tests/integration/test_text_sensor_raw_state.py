"""Integration test for TextSensor get_raw_state() and StringRef-based filters.

This tests:
1. The optimization in PR #12205 where raw_state is only stored when filters
   are configured. When no filters exist, get_raw_state() should return state.
2. StringRef-based filters (append, prepend, substitute, map) which store
   static string data in flash instead of heap-allocating std::string.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


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
    loop = asyncio.get_running_loop()

    # Futures to track log messages
    no_filter_future: asyncio.Future[tuple[str, str]] = loop.create_future()
    with_filter_future: asyncio.Future[tuple[str, str]] = loop.create_future()
    append_future: asyncio.Future[str] = loop.create_future()
    prepend_future: asyncio.Future[str] = loop.create_future()
    substitute_future: asyncio.Future[str] = loop.create_future()
    map_on_future: asyncio.Future[str] = loop.create_future()
    map_off_future: asyncio.Future[str] = loop.create_future()
    map_unknown_future: asyncio.Future[str] = loop.create_future()
    chained_future: asyncio.Future[str] = loop.create_future()
    to_lower_future: asyncio.Future[str] = loop.create_future()
    lambda_future: asyncio.Future[str] = loop.create_future()
    lambda_pass_future: asyncio.Future[str] = loop.create_future()
    lambda_skip_future: asyncio.Future[str] = loop.create_future()
    lambda_raw_state_future: asyncio.Future[tuple[str, str]] = loop.create_future()

    # Patterns to match log output
    # NO_FILTER: state='hello world' raw_state='hello world'
    no_filter_pattern = re.compile(r"NO_FILTER: state='([^']*)' raw_state='([^']*)'")
    # WITH_FILTER: state='HELLO WORLD' raw_state='hello world'
    with_filter_pattern = re.compile(
        r"WITH_FILTER: state='([^']*)' raw_state='([^']*)'"
    )
    # StringRef-based filter patterns
    append_pattern = re.compile(r"APPEND: state='([^']*)'")
    prepend_pattern = re.compile(r"PREPEND: state='([^']*)'")
    substitute_pattern = re.compile(r"SUBSTITUTE: state='([^']*)'")
    map_on_pattern = re.compile(r"MAP_ON: state='([^']*)'")
    map_off_pattern = re.compile(r"MAP_OFF: state='([^']*)'")
    map_unknown_pattern = re.compile(r"MAP_UNKNOWN: state='([^']*)'")
    chained_pattern = re.compile(r"CHAINED: state='([^']*)'")
    to_lower_pattern = re.compile(r"TO_LOWER: state='([^']*)'")
    lambda_pattern = re.compile(r"LAMBDA: state='([^']*)'")
    lambda_pass_pattern = re.compile(r"LAMBDA_PASS: state='([^']*)'")
    lambda_skip_pattern = re.compile(r"LAMBDA_SKIP: state='([^']*)'")
    lambda_raw_state_pattern = re.compile(
        r"LAMBDA_RAW_STATE: state='([^']*)' raw_state='([^']*)'"
    )

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if not no_filter_future.done() and (match := no_filter_pattern.search(line)):
            no_filter_future.set_result((match.group(1), match.group(2)))

        if not with_filter_future.done() and (
            match := with_filter_pattern.search(line)
        ):
            with_filter_future.set_result((match.group(1), match.group(2)))

        if not append_future.done() and (match := append_pattern.search(line)):
            append_future.set_result(match.group(1))

        if not prepend_future.done() and (match := prepend_pattern.search(line)):
            prepend_future.set_result(match.group(1))

        if not substitute_future.done() and (match := substitute_pattern.search(line)):
            substitute_future.set_result(match.group(1))

        if not map_on_future.done() and (match := map_on_pattern.search(line)):
            map_on_future.set_result(match.group(1))

        if not map_off_future.done() and (match := map_off_pattern.search(line)):
            map_off_future.set_result(match.group(1))

        if not map_unknown_future.done() and (
            match := map_unknown_pattern.search(line)
        ):
            map_unknown_future.set_result(match.group(1))

        if not chained_future.done() and (match := chained_pattern.search(line)):
            chained_future.set_result(match.group(1))

        if not to_lower_future.done() and (match := to_lower_pattern.search(line)):
            to_lower_future.set_result(match.group(1))

        if not lambda_future.done() and (match := lambda_pattern.search(line)):
            lambda_future.set_result(match.group(1))

        if not lambda_pass_future.done() and (
            match := lambda_pass_pattern.search(line)
        ):
            lambda_pass_future.set_result(match.group(1))

        if not lambda_skip_future.done() and (
            match := lambda_skip_pattern.search(line)
        ):
            lambda_skip_future.set_result(match.group(1))

        if not lambda_raw_state_future.done() and (
            match := lambda_raw_state_pattern.search(line)
        ):
            lambda_raw_state_future.set_result((match.group(1), match.group(2)))

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "test-text-sensor-raw-state"

        # Get entities to find our buttons
        entities, _ = await client.list_entities_services()

        # Find the test buttons
        no_filter_button = require_entity(entities, "test_no_filter_button")

        with_filter_button = require_entity(entities, "test_with_filter_button")

        # Test 1: Text sensor without filters
        # get_raw_state() should return the same as state
        client.button_command(no_filter_button.key)

        state, raw_state = await wait_or_fail(
            no_filter_future, 5.0, "Timeout waiting for NO_FILTER log message"
        )

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

        state, raw_state = await wait_or_fail(
            with_filter_future, 5.0, "Timeout waiting for WITH_FILTER log message"
        )

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

        state = await wait_or_fail(
            append_future, 5.0, "Timeout waiting for APPEND log message"
        )

        assert state == "test suffix", (
            f"Append failed: expected 'test suffix', got '{state}'"
        )

        # Test 4: Prepend filter (StringRef-based)
        # "prefix " + "test" = "prefix test"
        prepend_button = require_entity(entities, "test_prepend_button")
        client.button_command(prepend_button.key)

        state = await wait_or_fail(
            prepend_future, 5.0, "Timeout waiting for PREPEND log message"
        )

        assert state == "prefix test", (
            f"Prepend failed: expected 'prefix test', got '{state}'"
        )

        # Test 5: Substitute filter (StringRef-based)
        # "foo says hello" with foo->bar, hello->world = "bar says world"
        substitute_button = require_entity(entities, "test_substitute_button")
        client.button_command(substitute_button.key)

        state = await wait_or_fail(
            substitute_future, 5.0, "Timeout waiting for SUBSTITUTE log message"
        )

        assert state == "bar says world", (
            f"Substitute failed: expected 'bar says world', got '{state}'"
        )

        # Test 6: Map filter - "ON" -> "Active"
        map_on_button = require_entity(entities, "test_map_on_button")
        client.button_command(map_on_button.key)

        state = await wait_or_fail(
            map_on_future, 5.0, "Timeout waiting for MAP_ON log message"
        )

        assert state == "Active", f"Map ON failed: expected 'Active', got '{state}'"

        # Test 7: Map filter - "OFF" -> "Inactive"
        map_off_button = require_entity(entities, "test_map_off_button")
        client.button_command(map_off_button.key)

        state = await wait_or_fail(
            map_off_future, 5.0, "Timeout waiting for MAP_OFF log message"
        )

        assert state == "Inactive", (
            f"Map OFF failed: expected 'Inactive', got '{state}'"
        )

        # Test 8: Map filter - passthrough for unknown values
        # "UNKNOWN" -> "UNKNOWN" (no match, passes through unchanged)
        map_unknown_button = require_entity(entities, "test_map_unknown_button")
        client.button_command(map_unknown_button.key)

        state = await wait_or_fail(
            map_unknown_future, 5.0, "Timeout waiting for MAP_UNKNOWN log message"
        )

        assert state == "UNKNOWN", (
            f"Map passthrough failed: expected 'UNKNOWN', got '{state}'"
        )

        # Test 9: Chained filters (prepend "[" + append "]")
        # "[" + "value" + "]" = "[value]"
        chained_button = require_entity(entities, "test_chained_button")
        client.button_command(chained_button.key)

        state = await wait_or_fail(
            chained_future, 5.0, "Timeout waiting for CHAINED log message"
        )

        assert state == "[value]", f"Chained failed: expected '[value]', got '{state}'"

        # Test 10: to_lower filter
        # "HELLO WORLD" -> "hello world"
        to_lower_button = require_entity(entities, "test_to_lower_button")
        client.button_command(to_lower_button.key)

        state = await wait_or_fail(
            to_lower_future, 5.0, "Timeout waiting for TO_LOWER log message"
        )

        assert state == "hello world", (
            f"to_lower failed: expected 'hello world', got '{state}'"
        )

        # Test 11: Lambda filter
        # "test" -> "[test]"
        lambda_button = require_entity(entities, "test_lambda_button")
        client.button_command(lambda_button.key)

        state = await wait_or_fail(
            lambda_future, 5.0, "Timeout waiting for LAMBDA log message"
        )

        assert state == "[test]", f"Lambda failed: expected '[test]', got '{state}'"

        # Test 12: Lambda filter - value passes through
        # "value" -> "value passed"
        lambda_pass_button = require_entity(entities, "test_lambda_pass_button")
        client.button_command(lambda_pass_button.key)

        state = await wait_or_fail(
            lambda_pass_future, 5.0, "Timeout waiting for LAMBDA_PASS log message"
        )

        assert state == "value passed", (
            f"Lambda pass failed: expected 'value passed', got '{state}'"
        )

        # Test 13: Lambda filter - skip publishing (return {})
        # "skip" -> no publish, state remains "value passed" from previous test
        lambda_skip_button = require_entity(entities, "test_lambda_skip_button")
        client.button_command(lambda_skip_button.key)

        state = await wait_or_fail(
            lambda_skip_future, 5.0, "Timeout waiting for LAMBDA_SKIP log message"
        )

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

        state, raw_state = await wait_or_fail(
            lambda_raw_state_future,
            5.0,
            "Timeout waiting for LAMBDA_RAW_STATE log message",
        )

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
