"""Integration test for the script on_stop trigger, script.stop without an id, and the choose action."""

from __future__ import annotations

import asyncio

from aioesphomeapi import UserService
import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_script_on_stop_and_choose(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test on_stop, self-stopping scripts and choose."""
    waiter = LineWaiter()

    def count(text: str) -> int:
        return sum(text in line for line in waiter.lines)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        service: dict[str, UserService] = {s.name: s for s in services}

        # choose runs only the first choice whose condition is true, or the else
        # choice if none is, then continues
        for value, label in (
            (5, "small"),
            (50, "medium"),
            (500, "large"),
            (5000, "default"),
        ):
            await client.execute_service(service["run_choose"], {"value": value})
            await waiter.wait_for(f"CHOOSE done {value}", timeout=5.0)
            assert count(f"CHOOSE {label} {value}") == 1
        assert count("CHOOSE small") == 1
        assert count("CHOOSE medium") == 1
        assert count("CHOOSE large") == 1
        assert count("CHOOSE default") == 1

        # A bare script.stop inside a script stops that script and fires on_stop
        await client.execute_service(service["run_self_stop"], {})
        await waiter.wait_for("SELF on_stop", timeout=5.0)

        # Stopping a running script from outside fires on_stop
        await client.execute_service(service["run_external_stop"], {})
        await waiter.wait_for("LONG on_stop", timeout=5.0)

        # Stopping a script that is not running does not fire on_stop
        await client.execute_service(service["run_idle_stop"], {})
        await waiter.wait_for("IDLE stop done", timeout=5.0)

        # Restarting a running script stops the old run early
        await client.execute_service(service["run_restart"], {})
        await waiter.wait_for("RESTART finished", timeout=5.0)

        # A script that finishes normally does not fire on_stop
        await client.execute_service(service["run_normal"], {})
        await waiter.wait_for("NORMAL finished", timeout=5.0)

        # A restart script whose on_stop executes it again ends up with one run
        await client.execute_service(service["run_reenter"], {})
        await waiter.wait_for("REENTER finished", timeout=5.0)

        # Stopping several parallel runs fires on_stop once
        await client.execute_service(service["run_parallel_stop"], {})
        await waiter.wait_for("PARALLEL stop done", timeout=5.0)

        # Stopping a queued script fires on_stop once and drops the queued run
        await client.execute_service(service["run_queued_stop"], {})
        await waiter.wait_for("QUEUED stop done", timeout=5.0)
        await asyncio.sleep(0.2)

        assert count("SELF not reached") == 0
        assert count("SELF on_stop") == 1
        assert count("LONG finished") == 0
        assert count("LONG on_stop") == 1
        assert count("RESTART start") == 2
        assert count("RESTART on_stop") == 1
        assert count("RESTART finished") == 1
        assert count("NORMAL on_stop") == 0
        assert count("REENTER on_stop") == 1
        assert count("REENTER start") == 3
        assert count("REENTER finished") == 1
        assert count("PARALLEL on_stop") == 1
        assert count("PARALLEL finished") == 0
        assert count("QUEUED on_stop") == 1
        assert count("QUEUED start") == 1
        assert count("QUEUED finished") == 0
