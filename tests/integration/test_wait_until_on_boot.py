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

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_wait_until_on_boot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that wait_until works in on_boot automation with a condition that becomes true later."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Wait for on_boot to start
        await waiter.wait_for("on_boot: Starting wait_until test", timeout=10.0)

        # At this point, on_boot is blocked in wait_until waiting for test_flag to become true
        # If the bug exists, wait_until's loop is disabled and it will never complete
        # even after we set the flag

        # Give a moment for setup to complete
        await asyncio.sleep(0.5)

        # Now set the flag that wait_until is waiting for
        _, services = await client.list_entities_services()
        set_flag_service = require_service(services, "set_test_flag")

        await client.execute_service(set_flag_service, {})

        # If the fix works, wait_until's loop() will check the condition and proceed
        # If the bug exists, wait_until is stuck with disabled loop and will timeout
        await wait_or_fail(
            waiter.wait_for("on_boot: wait_until completed successfully"),
            2.0,
            "wait_until in on_boot did not complete within 2s after condition became true. "
            "This indicates the bug where WaitUntilAction::setup() disables the loop "
            "after play_complex() has already enabled it.",
        )
