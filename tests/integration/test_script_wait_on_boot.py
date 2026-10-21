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

import asyncio
import re

import pytest

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
    test_complete = asyncio.Event()

    # Track progress through the boot sequence
    boot_started = False
    first_script_started = False
    first_script_completed = False
    first_wait_returned = False
    second_script_started = False
    second_script_completed = False
    all_completed = False

    # Patterns for boot sequence logs
    boot_start_pattern = re.compile(r"on_boot: Starting boot sequence")
    show_start_pattern = re.compile(r"show_start_page: Starting")
    show_complete_pattern = re.compile(r"show_start_page: Completed")
    first_wait_pattern = re.compile(r"on_boot: First script completed")
    flip_start_pattern = re.compile(r"flip_thru_pages: Starting")
    flip_complete_pattern = re.compile(r"flip_thru_pages: Completed")
    all_complete_pattern = re.compile(r"on_boot: All boot scripts completed")

    def check_output(line: str) -> None:
        """Check log output for boot sequence progress."""
        nonlocal boot_started, first_script_started, first_script_completed
        nonlocal first_wait_returned, second_script_started, second_script_completed
        nonlocal all_completed

        if boot_start_pattern.search(line):
            boot_started = True
        elif show_start_pattern.search(line):
            first_script_started = True
        elif show_complete_pattern.search(line):
            first_script_completed = True
        elif first_wait_pattern.search(line):
            first_wait_returned = True
        elif flip_start_pattern.search(line):
            second_script_started = True
        elif flip_complete_pattern.search(line):
            second_script_completed = True
        elif all_complete_pattern.search(line):
            all_completed = True
            test_complete.set()

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "test-script-wait-on-boot")

        # Wait for on_boot sequence to complete
        # The boot sequence should complete automatically
        # Timeout is generous to allow for delays in the scripts
        def progress() -> list[str]:
            steps = (
                (boot_started, "boot started"),
                (first_script_started, "show_start_page started"),
                (first_script_completed, "show_start_page completed"),
                (first_wait_returned, "first script.wait returned"),
                (second_script_started, "flip_thru_pages started"),
                (second_script_completed, "flip_thru_pages completed"),
            )
            return [name for done, name in steps if done]

        await wait_or_fail(
            test_complete,
            5.0,
            "Boot sequence did not complete",
            progress=progress,
            # script.wait hanging after show_start_page completed is the issue #12043 bug
            script_wait_hung=lambda: first_script_completed and not first_wait_returned,
        )

        # Verify the complete boot sequence executed in order
        assert boot_started, "on_boot did not start"
        assert first_script_started, "show_start_page did not start"
        assert first_script_completed, "show_start_page did not complete"
        assert first_wait_returned, "First script.wait did not return"
        assert second_script_started, "flip_thru_pages did not start"
        assert second_script_completed, "flip_thru_pages did not complete"
        assert all_completed, "Boot sequence did not complete"
