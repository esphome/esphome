"""Integration test for scripts whose actions all finish before execute() returns."""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import UserService
import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_script_synchronous(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Script modes, stop, wait and parameters behave the same for a synchronous body."""
    waiter = LineWaiter()

    def count(text: str) -> int:
        return sum(text in line for line in waiter.lines)

    def in_order(*texts: str) -> bool:
        """Whether the first log lines containing ``texts`` come in that order."""
        first = [
            next(i for i, line in enumerate(waiter.lines) if text in line)
            for text in texts
        ]
        return first == sorted(first)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        service: dict[str, UserService] = {s.name: s for s in services}

        # single: executing itself from its body is refused, the body still finishes
        await client.execute_service(service["run_single"], {})
        await waiter.wait_for("SINGLE end", timeout=5.0)
        assert count("SINGLE start") == 1
        assert count("Script 'single_script' is already running! (mode: single)") == 1

        # restart: the nested run stops the outer one, which skips its remaining actions
        await client.execute_service(service["run_restart"], {})
        await waiter.wait_for("RESTART end", timeout=5.0)
        assert count("Script 'restart_script' restarting (mode: restart)") == 1
        assert count("RESTART start") == 2
        assert count("RESTART on_stop") == 1

        # queued: the run executed from the body waits for it, the third is refused
        await client.execute_service(service["run_queued"], {})
        await waiter.wait_for("QUEUED run 2", timeout=5.0)
        assert count("Script 'queued_script' queueing new instance (mode: queued)") == 1
        assert (
            count("Script 'queued_script' max instances (running + queued) reached!")
            == 1
        )
        assert in_order("QUEUED run 1", "queueing new instance", "QUEUED run 2")

        # parallel: a run nested in the body counts against max_runs
        await client.execute_service(service["run_parallel"], {})
        await waiter.wait_for("PARALLEL end 1", timeout=5.0)
        assert count("PARALLEL run 1") == 1
        assert count("PARALLEL run 2") == 1
        assert count("PARALLEL run 3") == 0
        assert count("PARALLEL end 2") == 1
        assert (
            count("Script 'parallel_script' maximum number of parallel runs exceeded!")
            == 1
        )

        # parameters: each lambda doubles its own copy of x
        await client.execute_service(
            service["run_params"], {"value": 5, "label": "hello"}
        )
        await waiter.wait_for("PARAMS second", timeout=5.0)
        assert count("PARAMS lambda 10 hello") == 1
        assert count("PARAMS again 10 hello") == 1
        assert count("PARAMS first 10") == 1
        assert count("PARAMS second 10") == 1

        # script.stop in the body skips the rest and fires on_stop
        await client.execute_service(service["run_stop"], {})
        await waiter.wait_for("STOP on_stop", timeout=5.0)

        # running while its body runs, so a script.wait from inside waits for the end;
        # script.wait after execute() returns does not wait at all
        await client.execute_service(service["run_wait"], {})
        await waiter.wait_for("WAITER done", timeout=5.0)
        assert in_order(
            "SYNC start", "SYNC running inside", "SYNC end", "WAIT done", "WAITER done"
        )

        # not running once execute() returns
        await client.execute_service(service["run_outside"], {})
        await waiter.wait_for("OUTSIDE", timeout=5.0)
        await waiter.wait_count("WAITER done", 2, timeout=5.0)
        assert count("OUTSIDE idle") == 1
        assert count("OUTSIDE running") == 0

        # a parallel script can call itself; the innermost run ends first
        await client.execute_service(service["run_recursive"], {})
        await waiter.wait_for("RECURSIVE 3", timeout=5.0)
        recursive = [
            match.group(1)
            for line in waiter.lines
            if (match := re.search(r"RECURSIVE (\d)", line))
        ]
        assert recursive == ["0", "1", "2", "3"]

        await asyncio.sleep(0.2)
        assert count("STOP start") == 1
        assert count("STOP not reached") == 0
        assert count("STOP on_stop") == 1
        assert count("SINGLE start") == 1
        assert count("RESTART end") == 1
        assert count("QUEUED run 3") == 0
        assert count("WAITER done") == 2
