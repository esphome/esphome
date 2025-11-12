"""Integration test for wait_until in on_boot automation.

This test validates that wait_until works correctly when triggered from on_boot,
which runs at the same setup priority as WaitUntilAction itself. This was broken
before the fix because WaitUntilAction::setup() would unconditionally disable_loop(),
even if play_complex() had already been called and enabled the loop.

The bug: on_boot fires during StartupTrigger::setup(), which calls WaitUntilAction::play_complex()
before WaitUntilAction::setup() has run. Then when WaitUntilAction::setup() runs, it calls
disable_loop(), undoing the enable_loop() from play_complex(), causing wait_until to hang forever.
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
    """Test that wait_until works in on_boot automation with a condition that becomes true later."""
    loop = asyncio.get_running_loop()

    on_boot_started = False
    on_boot_completed = False

    on_boot_started_pattern = re.compile(r"on_boot: Starting wait_until test")
    on_boot_complete_pattern = re.compile(r"on_boot: wait_until completed successfully")

    on_boot_started_future = loop.create_future()
    on_boot_complete_future = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for test progress."""
        nonlocal on_boot_started, on_boot_completed

        if on_boot_started_pattern.search(line):
            on_boot_started = True
            if not on_boot_started_future.done():
                on_boot_started_future.set_result(True)

        if on_boot_complete_pattern.search(line):
            on_boot_completed = True
            if not on_boot_complete_future.done():
                on_boot_complete_future.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Wait for on_boot to start
        await asyncio.wait_for(on_boot_started_future, timeout=10.0)
        assert on_boot_started, "on_boot did not start"

        # At this point, on_boot is blocked in wait_until waiting for test_flag to become true
        # If the bug exists, wait_until's loop is disabled and it will never complete
        # even after we set the flag

        # Give a moment for setup to complete
        await asyncio.sleep(0.5)

        # Now set the flag that wait_until is waiting for
        _, services = await client.list_entities_services()
        set_flag_service = next(
            (s for s in services if s.name == "set_test_flag"), None
        )
        assert set_flag_service is not None, "set_test_flag service not found"

        client.execute_service(set_flag_service, {})

        # If the fix works, wait_until's loop() will check the condition and proceed
        # If the bug exists, wait_until is stuck with disabled loop and will timeout
        try:
            await asyncio.wait_for(on_boot_complete_future, timeout=2.0)
            assert on_boot_completed, (
                "on_boot wait_until did not complete after flag was set"
            )
        except TimeoutError:
            pytest.fail(
                "wait_until in on_boot did not complete within 2s after condition became true. "
                "This indicates the bug where WaitUntilAction::setup() disables the loop "
                "after play_complex() has already enabled it."
            )
