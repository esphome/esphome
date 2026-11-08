"""Integration test for script.wait during on_boot (issue #12043).

This test verifies that script.wait works correctly when triggered from on_boot.
The issue was that ScriptWaitAction::setup() unconditionally disabled the loop,
even if play_complex() had already been called (from an on_boot trigger at the
same priority level) and enabled it.

The race condition occurs because:
1. on_boot's default priority is 600.0 (setup_priority::DATA)
2. ScriptWaitAction's default setup priority is also DATA (600.0)
3. When they have the same priority, if on_boot runs first and triggers a script,
   ScriptWaitAction::play_complex() enables the loop
4. Then ScriptWaitAction::setup() runs and unconditionally disables the loop
5. The wait never completes because the loop is disabled

The fix adds a conditional check (like WaitUntilAction has) to only disable the
loop in setup() if num_running_ is 0.
"""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_script_wait_on_boot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that script.wait works correctly when triggered from on_boot.

    This reproduces issue #12043 where script.wait would hang forever when
    triggered from on_boot due to a race condition in ScriptWaitAction::setup().
    """
    # Boot sequence log lines, in order
    boot_start = "on_boot: Starting boot sequence"
    show_start = "show_start_page: Starting"
    show_complete = "show_start_page: Completed"
    first_wait = "on_boot: First script completed"
    flip_start = "flip_thru_pages: Starting"
    flip_complete = "flip_thru_pages: Completed"

    waiter = LineWaiter()
    test_complete = waiter.expect("on_boot: All boot scripts completed")

    def seen(text: str) -> bool:
        return waiter.count(text) > 0

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "test-script-wait-on-boot")

        # Wait for on_boot sequence to complete
        # The boot sequence should complete automatically
        # Timeout is generous to allow for delays in the scripts
        def progress() -> list[str]:
            steps = (
                (boot_start, "boot started"),
                (show_start, "show_start_page started"),
                (show_complete, "show_start_page completed"),
                (first_wait, "first script.wait returned"),
                (flip_start, "flip_thru_pages started"),
                (flip_complete, "flip_thru_pages completed"),
            )
            return [name for text, name in steps if seen(text)]

        await wait_or_fail(
            test_complete,
            5.0,
            "Boot sequence did not complete",
            progress=progress,
            issue_12043_script_wait_hung=lambda: (
                seen(show_complete) and not seen(first_wait)
            ),
        )

        # Verify the complete boot sequence executed in order
        assert seen(boot_start), "on_boot did not start"
        assert seen(show_start), "show_start_page did not start"
        assert seen(show_complete), "show_start_page did not complete"
        assert seen(first_wait), "First script.wait did not return"
        assert seen(flip_start), "flip_thru_pages did not start"
        assert seen(flip_complete), "flip_thru_pages did not complete"
