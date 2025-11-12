"""Integration test for wait_until in on_boot automation.

This test validates that wait_until works correctly when triggered from on_boot,
which runs at the same setup priority as WaitUntilAction itself. This was broken
before the fix because WaitUntilAction::setup() would unconditionally disable_loop(),
even if play_complex() had already been called and enabled the loop.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_wait_until_on_boot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that wait_until works in on_boot automation."""
    loop = asyncio.get_running_loop()

    # Track if on_boot completed successfully
    on_boot_completed = False
    action_completed = False

    on_boot_complete_pattern = re.compile(r"on_boot: wait_until completed successfully")
    action_complete_pattern = re.compile(r"Action: wait_until completed")

    on_boot_future = loop.create_future()
    action_future = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for completion messages."""
        nonlocal on_boot_completed, action_completed

        if on_boot_complete_pattern.search(line):
            on_boot_completed = True
            if not on_boot_future.done():
                on_boot_future.set_result(True)

        if action_complete_pattern.search(line):
            action_completed = True
            if not action_future.done():
                action_future.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Wait for on_boot to complete (should happen within a few seconds)
        await asyncio.wait_for(on_boot_future, timeout=10.0)
        assert on_boot_completed, "on_boot wait_until did not complete"

        # Test that wait_until also works from regular actions
        _, services = await client.list_entities_services()
        test_service = next(
            (s for s in services if s.name == "test_wait_until_on_boot"), None
        )
        assert test_service is not None, "test_wait_until_on_boot service not found"

        client.execute_service(test_service, {})
        await asyncio.wait_for(action_future, timeout=10.0)
        assert action_completed, "Action wait_until did not complete"
