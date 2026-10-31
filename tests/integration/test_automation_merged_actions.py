"""Integration test that merged adjacent actions behave like a chain of separate actions."""

from __future__ import annotations

import re

from aioesphomeapi import UserService
import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_automation_merged_actions(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Stops, re-entry, run counts, lambda scope and argument names are all unchanged."""
    waiter = LineWaiter()

    def count(text: str) -> int:
        return sum(text in line for line in waiter.lines)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        service: dict[str, UserService] = {s.name: s for s in services}

        # A stop during one action keeps the following actions from running, whichever way
        # it is reached, and execution returns to the caller.
        for action, prefix in (
            ("run_stop_in_lambda", "STOP_LAMBDA"),
            ("run_stop_by_action", "STOP_ACTION"),
            ("run_stop_by_callback", "STOP_CALLBACK"),
        ):
            await client.execute_service(service[action], {})
            await waiter.wait_for(f"{prefix} done", timeout=5.0)
            assert count(f"{prefix} before") == 1
            assert count(f"{prefix} after") == 0

        await client.execute_service(service["run_stop_in_branch"], {})
        await waiter.wait_for("STOP_BRANCH done", timeout=5.0)
        assert count("STOP_BRANCH before") == 1
        assert count("STOP_BRANCH after") == 0

        # A restart from inside the run stops the outer run; only the inner one goes on
        await client.execute_service(service["run_restart_reentry"], {})
        await waiter.wait_for("RESTART end 1", timeout=5.0)
        await client.execute_service(service["run_is_running"], {})
        await waiter.wait_for("RUNNING after", timeout=5.0)
        assert count("RESTART start 0") == 1
        assert count("RESTART start 1") == 1
        assert count("RESTART middle 1") == 1
        assert count("RESTART middle 0") == 0
        assert count("RESTART end 0") == 0

        # A run holds one slot: with max_runs 1 the re-entrant execute is rejected
        assert count("RUNNING during 1") == 1
        assert count("RUNNING after 0") == 1
        await client.execute_service(service["run_parallel_limit"], {})
        await waiter.wait_for("PARALLEL done", timeout=5.0)
        assert count("PARALLEL start") == 1
        assert count("PARALLEL end") == 1
        assert count("maximum number of parallel runs exceeded") == 1

        # A return ends only its own lambda, an argument change stays in that lambda, and a
        # static local keeps one value across runs.
        await client.execute_service(service["run_lambda_scope"], {})
        await waiter.wait_for("SCOPE done", timeout=5.0)
        await waiter.wait_for("SCOPE second abc 2", timeout=5.0)
        assert count("SCOPE first abc!") == 2
        assert count("SCOPE second abc 1") == 1

        # A parameter or a global named `running` does not clash with the generated code
        await client.execute_service(service["run_named_running"], {})
        await waiter.wait_for("NAMED done", timeout=5.0)
        assert count("NAMED param first 5") == 1
        assert count("NAMED param second 5") == 1
        assert count("NAMED global first 7") == 1
        assert count("NAMED global second 7") == 1

        # Merged actions inside a repeat see its iteration, in order
        await client.execute_service(service["run_repeat"], {})
        await waiter.wait_for("REPEAT done", timeout=5.0)
        order = [
            match.group(1)
            for line in waiter.lines
            if (match := re.search(r"REPEAT (first \d|second \d)", line))
        ]
        assert order == ["first 0", "second 0", "first 1", "second 1"]

        # Arguments reach the merged actions on both sides of a delay
        await client.execute_service(service["run_args"], {"value": 42})
        await waiter.wait_for("ARGS fourth 42", timeout=5.0)
        for label in ("first", "second", "third"):
            assert count(f"ARGS {label} 42") == 1
