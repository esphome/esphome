"""Test that suspending/resuming the preferences IntervalSyncer actually stops/starts flash writes."""

from __future__ import annotations

import asyncio
from pathlib import Path
import re
from typing import Any

from aioesphomeapi import ButtonInfo, EntityInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "test_suspend_resume_device"


def find_entity_by_name(
    entities: list[EntityInfo], entity_type: type, name: str
) -> Any:
    """Helper to find an entity by type and name."""
    return next(
        (e for e in entities if isinstance(e, entity_type) and e.name == name), None
    )


@pytest.mark.asyncio
async def test_host_preferences_suspend_resume(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that a suspended IntervalSyncer skips flash writes, and resume restores them."""
    pref_file = Path.home() / ".esphome" / "prefs" / f"{DEVICE_NAME}.prefs"
    pref_file.unlink(missing_ok=True)

    loop = asyncio.get_running_loop()
    saved_in_memory = loop.create_future()
    syncer_resumed = loop.create_future()

    save_pattern = re.compile(r"Preference saved in memory")
    resume_pattern = re.compile(r"Syncer resumed")

    def check_output(line: str) -> None:
        if save_pattern.search(line) and not saved_in_memory.done():
            saved_in_memory.set_result(True)
        if resume_pattern.search(line) and not syncer_resumed.done():
            syncer_resumed.set_result(True)

    try:
        async with (
            run_compiled(yaml_config, line_callback=check_output),
            api_client_connected() as client,
        ):
            entities, _ = await client.list_entities_services()

            save_button = find_entity_by_name(entities, ButtonInfo, "Save Preference")
            resume_button = find_entity_by_name(entities, ButtonInfo, "Resume Syncer")
            assert save_button is not None, "Save Preference button not found"
            assert resume_button is not None, "Resume Syncer button not found"

            # Write a preference while the syncer is suspended (from on_boot).
            client.button_command(save_button.key)
            try:
                await asyncio.wait_for(saved_in_memory, timeout=5.0)
            except TimeoutError:
                pytest.fail("Preference was not saved to memory within timeout")

            # Wait well past flash_write_interval (1s): a running syncer would
            # have flushed to disk by now, a suspended one must not have.
            await asyncio.sleep(1.5)
            assert not pref_file.exists(), (
                "Suspended syncer flushed to disk; component.suspend did not stop the poller"
            )

            # Resume the syncer and wait past the update interval again.
            client.button_command(resume_button.key)
            try:
                await asyncio.wait_for(syncer_resumed, timeout=5.0)
            except TimeoutError:
                pytest.fail("Syncer resume command was not processed within timeout")

            await asyncio.sleep(1.5)
            assert pref_file.exists(), (
                "Resumed syncer never flushed to disk; component.resume did not restart the poller"
            )
    finally:
        pref_file.unlink(missing_ok=True)
