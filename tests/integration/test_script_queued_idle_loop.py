"""Test that an idle queued script disables its loop and re-enables on demand."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_script_queued_idle_loop(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Assert the loop state transitions of a queued script via VV logs.

    Expected sequence: the idle script disables its loop on the first
    iteration after boot, re-enables it when an instance gets queued,
    and disables it again once the queue drains.
    """
    loop = asyncio.get_running_loop()

    loop_state = re.compile(r"\bscript loop (disabled|enabled)\b")
    script_end = re.compile(r"idle_script: END")

    transitions: list[str] = []
    end_count = 0

    boot_disabled = loop.create_future()
    enabled_after_queue = loop.create_future()
    disabled_after_drain = loop.create_future()
    runs_complete = loop.create_future()

    def check_output(line: str) -> None:
        nonlocal end_count
        if match := loop_state.search(line):
            transitions.append(match.group(1))
            if transitions == ["disabled"] and not boot_disabled.done():
                boot_disabled.set_result(True)
            elif (
                transitions == ["disabled", "enabled"]
                and not enabled_after_queue.done()
            ):
                enabled_after_queue.set_result(True)
            elif (
                transitions
                == [
                    "disabled",
                    "enabled",
                    "disabled",
                ]
                and not disabled_after_drain.done()
            ):
                disabled_after_drain.set_result(True)

        if script_end.search(line):
            end_count += 1
            if end_count == 2 and not runs_complete.done():
                runs_complete.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # The idle script must disable its loop on the first iteration
        await asyncio.wait_for(boot_disabled, timeout=5.0)

        _, services = await client.list_entities_services()
        run_twice = next((s for s in services if s.name == "run_twice"), None)
        assert run_twice is not None, "run_twice service not found"
        await client.execute_service(run_twice, {})

        # Queueing the second instance must re-enable the loop
        await asyncio.wait_for(enabled_after_queue, timeout=2.0)
        # Both runs must complete and the drained queue must disable it again
        await asyncio.wait_for(runs_complete, timeout=2.0)
        await asyncio.wait_for(disabled_after_drain, timeout=2.0)

        assert transitions == ["disabled", "enabled", "disabled"], (
            f"Unexpected loop state sequence: {transitions}"
        )
