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

    # Patterns to match in logs
    value_pattern = re.compile(r"Select value: Option B")
    concatenated_pattern = re.compile(r"Concatenated: Option B selected")
    comparison_pattern = re.compile(r"Option B was selected")
    index_pattern = re.compile(r"Select index: 1")

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

        # Change select to Option B - this should trigger on_value with StringRef
        client.select_command(select_entity.key, "Option B")

        # Wait for all log messages confirming StringRef operations work
        try:
            await asyncio.wait_for(
                asyncio.gather(
                    value_logged_future,
                    concatenated_future,
                    comparison_future,
                    index_logged_future,
                ),
                timeout=5.0,
            )
        except TimeoutError:
            results = {
                "value_logged": value_logged_future.done(),
                "concatenated": concatenated_future.done(),
                "comparison": comparison_future.done(),
                "index_logged": index_logged_future.done(),
            }
            pytest.fail(f"StringRef operations failed - received: {results}")
