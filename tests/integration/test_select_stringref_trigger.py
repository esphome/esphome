"""Integration test for select on_value trigger with StringRef parameter."""

from __future__ import annotations

import asyncio

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_select_stringref_trigger(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test select on_value trigger passes StringRef that works with string operations."""
    # Log lines confirming each StringRef operation works
    expected_lines = {
        "value_logged": "Select value: Option B",
        "concatenated": "Concatenated: Option B selected",
        "comparison": "Option B was selected",
        "index_logged": "Select index: 1",
        "length": "Length: 8",  # "Option B" is 8 chars
        "find_substr": "Found 'Option' in value",
        "find_char": "Space at position: 6",  # space at index 6
        "substr": "Substr prefix: Option",
        # set_action trigger (TemplateSelectWithSetAction subclass)
        "set_action": "set_action triggered: Action B",
        # ADL functions (115200 from baud rate select)
        "stoi": "stoi result: 115200",
        "stol": "stol result: 115200",
        "stof": "stof result: 115200",
        "stod": "stod result: 115200",
    }
    waiter = LineWaiter()
    futures = {name: waiter.expect(text) for name, text in expected_lines.items()}

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "select-stringref-test")

        # List entities to find our select
        entities, _ = await client.list_entities_services()

        select_entity = require_entity(entities, "test_select", exact=True)

        baud_entity = require_entity(entities, "baud_rate", exact=True)

        action_entity = require_entity(entities, "action_select", exact=True)

        # Change select to Option B - this should trigger on_value with StringRef
        client.select_command(select_entity.key, "Option B")
        # Change baud to 115200 - this tests ADL functions (stoi, stol, stof, stod)
        client.select_command(baud_entity.key, "115200")
        # Change action select - tests set_action trigger (TemplateSelectWithSetAction)
        client.select_command(action_entity.key, "Action B")

        # Wait for all log messages confirming StringRef operations work
        await wait_or_fail(
            asyncio.gather(*futures.values()),
            5.0,
            "StringRef operations failed",
            received=lambda: {name: f.done() for name, f in futures.items()},
        )
