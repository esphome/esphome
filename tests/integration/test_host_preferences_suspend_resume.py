"""Test that suspending/resuming the preferences IntervalSyncer actually stops/starts flash writes."""

from __future__ import annotations

import asyncio
from collections.abc import Awaitable
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


async def _wait_for(
    awaitable: Awaitable[Any], message: str, timeout: float = 5.0
) -> None:
    """Await a future or coroutine, failing the test with a clear message on timeout."""
    try:
        await asyncio.wait_for(awaitable, timeout=timeout)
    except TimeoutError:
        pytest.fail(message)


async def _poll_until_exists(path: Path) -> None:
    """Poll for a file to appear, rather than guessing a sleep duration."""
    while not path.exists():
        await asyncio.sleep(0.05)


@pytest.fixture(autouse=True)
def isolated_preferences(monkeypatch: pytest.MonkeyPatch, tmp_path) -> Path:
    """Keep host preferences per-test so this test never touches the real
    ~/.esphome/prefs and never races other tests over ESPHOME_PREFDIR."""
    prefdir = tmp_path / "prefs"
    monkeypatch.setenv("ESPHOME_PREFDIR", str(prefdir))
    return prefdir / f"{DEVICE_NAME}.prefs"


@pytest.mark.asyncio
async def test_host_preferences_suspend_resume(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    isolated_preferences: Path,
) -> None:
    """Test that a running syncer flushes, a suspended one doesn't, and resume restores flushing."""
    pref_file = isolated_preferences

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

        save_button = find_entity_by_name(entities, ButtonInfo, "Save Preference")
        suspend_button = find_entity_by_name(entities, ButtonInfo, "Suspend Syncer")
        resume_button = find_entity_by_name(entities, ButtonInfo, "Resume Syncer")
        assert save_button is not None, "Save Preference button not found"
        assert suspend_button is not None, "Suspend Syncer button not found"
        assert resume_button is not None, "Resume Syncer button not found"

        # --- Positive control: a running syncer flushes to disk. Without this,
        # the suspend assertion below could pass for the wrong reason (e.g. wrong prefs path). ---
        client.button_command(save_button.key)
        await _wait_for(
            saved_in_memory, "Preference was not saved to memory within timeout"
        )
        await _wait_for(
            _poll_until_exists(pref_file),
            "Running syncer never flushed to disk; positive control failed",
            timeout=10.0,
        )
        saved_in_memory = loop.create_future()

        # --- Suspend: a suspended syncer must not flush. ---
        client.button_command(suspend_button.key)
        await _wait_for(
            syncer_suspended, "Syncer suspend command was not processed within timeout"
        )
        # Delete only after suspend is confirmed: the poller is now stopped, so
        # nothing can recreate the file before the negative assertion below.
        pref_file.unlink()

        client.button_command(save_button.key)
        await _wait_for(
            saved_in_memory, "Preference was not saved to memory within timeout"
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
        await _wait_for(
            syncer_resumed, "Syncer resume command was not processed within timeout"
        )
        await _wait_for(
            _poll_until_exists(pref_file),
            "Resumed syncer never flushed to disk; component.resume did not restart the poller",
            timeout=10.0,
        )
