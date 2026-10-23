"""Test that suspending/resuming the preferences IntervalSyncer actually stops/starts flash writes."""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

DEVICE_NAME = "test_suspend_resume_device"


async def _poll_until_exists(path: Path) -> None:
    """Poll for a file to appear, rather than guessing a sleep duration."""
    while not path.exists():
        await asyncio.sleep(0.05)


@pytest.mark.asyncio
async def test_host_preferences_suspend_resume(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    isolated_preferences: Path,
) -> None:
    """Test that a running syncer flushes, a suspended one doesn't, and resume restores flushing."""
    pref_file = isolated_preferences / f"{DEVICE_NAME}.prefs"

    loop = asyncio.get_running_loop()
    saved_in_memory = loop.create_future()
    syncer_suspended = loop.create_future()
    syncer_resumed = loop.create_future()

    save_pattern = re.compile(r"Preference saved in memory")
    suspend_pattern = re.compile(r"Syncer suspended")
    resume_pattern = re.compile(r"Syncer resumed")

    def check_output(line: str) -> None:
        if save_pattern.search(line) and not saved_in_memory.done():
            saved_in_memory.set_result(True)
        if suspend_pattern.search(line) and not syncer_suspended.done():
            syncer_suspended.set_result(True)
        if resume_pattern.search(line) and not syncer_resumed.done():
            syncer_resumed.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        save_button = require_entity(
            entities, "save_preference", ButtonInfo, exact=True
        )
        suspend_button = require_entity(
            entities, "suspend_syncer", ButtonInfo, exact=True
        )
        resume_button = require_entity(
            entities, "resume_syncer", ButtonInfo, exact=True
        )

        # --- Positive control: a running syncer flushes to disk. Without this,
        # the suspend assertion below could pass for the wrong reason (e.g. wrong prefs path). ---
        client.button_command(save_button.key)
        await wait_or_fail(
            saved_in_memory, 5.0, "Preference was not saved to memory within timeout"
        )
        await wait_or_fail(
            _poll_until_exists(pref_file),
            10.0,
            "Running syncer never flushed to disk; positive control failed",
        )
        saved_in_memory = loop.create_future()

        # --- Suspend: a suspended syncer must not flush. ---
        client.button_command(suspend_button.key)
        await wait_or_fail(
            syncer_suspended,
            5.0,
            "Syncer suspend command was not processed within timeout",
        )
        # Delete only after suspend is confirmed: the poller is now stopped, so
        # nothing can recreate the file before the negative assertion below.
        pref_file.unlink()

        client.button_command(save_button.key)
        await wait_or_fail(
            saved_in_memory, 5.0, "Preference was not saved to memory within timeout"
        )

        # Wait well past flash_write_interval (1s): a running syncer would
        # have flushed to disk by now, a suspended one must not have. This is a
        # negative assertion (proving absence), so a fixed sleep is unavoidable here.
        await asyncio.sleep(1.5)
        assert not pref_file.exists(), (
            "Suspended syncer flushed to disk; component.suspend did not stop the poller"
        )

        # --- Resume: flushing must restart. ---
        client.button_command(resume_button.key)
        await wait_or_fail(
            syncer_resumed,
            5.0,
            "Syncer resume command was not processed within timeout",
        )
        await wait_or_fail(
            _poll_until_exists(pref_file),
            10.0,
            "Resumed syncer never flushed to disk; component.resume did not restart the poller",
        )
