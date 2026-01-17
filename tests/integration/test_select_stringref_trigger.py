"""Integration test for select on_value trigger with StringRef parameter."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_select_stringref_trigger(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test select on_value trigger passes StringRef that works with string operations."""
    loop = asyncio.get_running_loop()

    # Track log messages to verify StringRef operations work
    value_logged_future = loop.create_future()
    concatenated_future = loop.create_future()
    comparison_future = loop.create_future()
    index_logged_future = loop.create_future()
    length_future = loop.create_future()
    find_substr_future = loop.create_future()
    find_char_future = loop.create_future()
    substr_future = loop.create_future()
    # ADL functions
    stoi_future = loop.create_future()
    stol_future = loop.create_future()
    stof_future = loop.create_future()
    stod_future = loop.create_future()

    # Patterns to match in logs
    value_pattern = re.compile(r"Select value: Option B")
    concatenated_pattern = re.compile(r"Concatenated: Option B selected")
    comparison_pattern = re.compile(r"Option B was selected")
    index_pattern = re.compile(r"Select index: 1")
    length_pattern = re.compile(r"Length: 8")  # "Option B" is 8 chars
    find_substr_pattern = re.compile(r"Found 'Option' in value")
    find_char_pattern = re.compile(r"Space at position: 6")  # space at index 6
    substr_pattern = re.compile(r"Substr prefix: Option")
    # ADL function patterns (115200 from baud rate select)
    stoi_pattern = re.compile(r"stoi result: 115200")
    stol_pattern = re.compile(r"stol result: 115200")
    stof_pattern = re.compile(r"stof result: 115200")
    stod_pattern = re.compile(r"stod result: 115200")

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if not value_logged_future.done() and value_pattern.search(line):
            value_logged_future.set_result(True)
        if not concatenated_future.done() and concatenated_pattern.search(line):
            concatenated_future.set_result(True)
        if not comparison_future.done() and comparison_pattern.search(line):
            comparison_future.set_result(True)
        if not index_logged_future.done() and index_pattern.search(line):
            index_logged_future.set_result(True)
        if not length_future.done() and length_pattern.search(line):
            length_future.set_result(True)
        if not find_substr_future.done() and find_substr_pattern.search(line):
            find_substr_future.set_result(True)
        if not find_char_future.done() and find_char_pattern.search(line):
            find_char_future.set_result(True)
        if not substr_future.done() and substr_pattern.search(line):
            substr_future.set_result(True)
        # ADL functions
        if not stoi_future.done() and stoi_pattern.search(line):
            stoi_future.set_result(True)
        if not stol_future.done() and stol_pattern.search(line):
            stol_future.set_result(True)
        if not stof_future.done() and stof_pattern.search(line):
            stof_future.set_result(True)
        if not stod_future.done() and stod_pattern.search(line):
            stod_future.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "select-stringref-test"

        # List entities to find our select
        entities, _ = await client.list_entities_services()

        select_entity = next(
            (e for e in entities if hasattr(e, "options") and e.name == "Test Select"),
            None,
        )
        assert select_entity is not None, "Test Select entity not found"

        baud_entity = next(
            (e for e in entities if hasattr(e, "options") and e.name == "Baud Rate"),
            None,
        )
        assert baud_entity is not None, "Baud Rate entity not found"

        # Change select to Option B - this should trigger on_value with StringRef
        client.select_command(select_entity.key, "Option B")
        # Change baud to 115200 - this tests ADL functions (stoi, stol, stof, stod)
        client.select_command(baud_entity.key, "115200")

        # Wait for all log messages confirming StringRef operations work
        try:
            await asyncio.wait_for(
                asyncio.gather(
                    value_logged_future,
                    concatenated_future,
                    comparison_future,
                    index_logged_future,
                    length_future,
                    find_substr_future,
                    find_char_future,
                    substr_future,
                    stoi_future,
                    stol_future,
                    stof_future,
                    stod_future,
                ),
                timeout=5.0,
            )
        except TimeoutError:
            results = {
                "value_logged": value_logged_future.done(),
                "concatenated": concatenated_future.done(),
                "comparison": comparison_future.done(),
                "index_logged": index_logged_future.done(),
                "length": length_future.done(),
                "find_substr": find_substr_future.done(),
                "find_char": find_char_future.done(),
                "substr": substr_future.done(),
                "stoi": stoi_future.done(),
                "stol": stol_future.done(),
                "stof": stof_future.done(),
                "stod": stod_future.done(),
            }
            pytest.fail(f"StringRef operations failed - received: {results}")
