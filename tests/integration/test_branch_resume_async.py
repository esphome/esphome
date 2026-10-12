"""Test that if/else and while branches resume correctly after async actions, stop and parallel runs."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_branch_resume_async(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A branch that ends after a delay must resume its owner exactly once."""
    lines: list[str] = []
    waiters: list[tuple[str, asyncio.Future[None]]] = []

    def has_marker(text: str, line: str) -> bool:
        # Whole markers only: "after-if" must not match "queued-after-if-1"
        return re.search(rf"(?<![\w-]){re.escape(text)}(?![\w-])", line) is not None

    def check_output(line: str) -> None:
        lines.append(line)
        for text, future in waiters:
            if has_marker(text, line) and not future.done():
                future.set_result(None)

    def wait_for(text: str) -> asyncio.Future[None]:
        future: asyncio.Future[None] = asyncio.get_running_loop().create_future()
        waiters.append((text, future))
        return future

    def count(text: str) -> int:
        return sum(has_marker(text, line) for line in lines)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        service = {s.name: s for s in services}

        done = wait_for("after-if")
        await client.execute_service(service["run_delay_in_if"], {})
        await asyncio.wait_for(done, timeout=2.0)
        assert count("if-branch-after-delay") == 1
        assert count("if-else-should-not-run") == 0
        assert count("after-if") == 1

        done = wait_for("after-else")
        await client.execute_service(service["run_delay_in_else"], {})
        await asyncio.wait_for(done, timeout=2.0)
        assert count("else-branch-after-delay") == 1
        assert count("if-then-should-not-run") == 0
        assert count("after-else") == 1

        done = wait_for("after-while")
        await client.execute_service(service["run_delay_in_while"], {})
        await asyncio.wait_for(done, timeout=2.0)
        assert [count(f"while-body-{i}") for i in (1, 2, 3)] == [1, 1, 1]
        assert count("while-body-4") == 0
        assert count("after-while") == 1

        done = wait_for("stop-check-done")
        await client.execute_service(service["run_stop_in_branch"], {})
        await asyncio.wait_for(done, timeout=3.0)
        assert count("stop-running=0") == 1
        assert count("stopped-branch-should-not-run") == 0
        assert count("stopped-after-if-should-not-run") == 0

        done = wait_for("after-nested-while")
        await client.execute_service(service["run_nested_if_last_in_while"], {})
        await asyncio.wait_for(done, timeout=2.0)
        assert [count(f"nested-if-in-while-{i}") for i in (1, 2)] == [1, 1]
        assert count("nested-if-in-while-3") == 0
        assert count("after-nested-while") == 1

        # Stop during the second iteration's delay: that iteration and the line after the loop never run;
        # a fresh run then completes
        done = wait_for("stop-while-done")
        await client.execute_service(service["run_stop_mid_while"], {})
        await asyncio.wait_for(done, timeout=3.0)
        assert count("stop-while-running=0") == 1
        assert count("stop-while-body-1") == 2
        assert [count(f"stop-while-body-{i}") for i in (2, 3, 4, 5)] == [1, 1, 1, 1]
        assert count("stop-while-done") == 1

        done = wait_for("queued-after-if-3")
        await client.execute_service(service["run_queued_branch"], {})
        await asyncio.wait_for(done, timeout=2.0)
        queued = [line for line in lines if "queued-" in line]
        order = [
            "queued-branch-1",
            "queued-after-if-1",
            "queued-branch-2",
            "queued-after-if-2",
            "queued-branch-3",
            "queued-after-if-3",
        ]
        assert [next(o for o in order if o in line) for line in queued] == order

        done = wait_for("restart-after-if-2")
        await client.execute_service(service["run_restart_branch"], {})
        await asyncio.wait_for(done, timeout=2.0)
        await asyncio.sleep(0.2)
        assert count("restart-branch-1") == 0
        assert count("restart-after-if-1") == 0
        assert count("restart-branch-2") == 1
        assert count("restart-after-if-2") == 1

        done = wait_for("after-delay-last")
        await client.execute_service(service["run_delay_last_in_branch"], {})
        await asyncio.wait_for(done, timeout=2.0)
        assert count("delay-last-start") == 1
        assert count("after-delay-last") == 1

        done = wait_for("parallel-after-if-3")
        await client.execute_service(service["run_parallel_branch"], {})
        await asyncio.wait_for(done, timeout=2.0)
        await asyncio.sleep(0.2)
        for run in (1, 2, 3):
            assert count(f"parallel-branch-done-{run}") == 1
            assert count(f"parallel-after-if-{run}") == 1

        # A condition that stops its own script must not start a branch or continue the chain
        done = wait_for("stop-in-conditions-done")
        await client.execute_service(service["run_stop_in_conditions"], {})
        await asyncio.wait_for(done, timeout=2.0)
        assert count("if-stopped-by-condition-should-not-run") == 0
        assert count("if-else-stopped-by-condition-should-not-run") == 0
        assert count("after-if-stopped-by-condition-should-not-run") == 0
        assert count("else-stopped-by-condition-should-not-run") == 0
        assert count("after-else-stopped-by-condition-should-not-run") == 0
        assert count("while-stopped-by-condition-should-not-run") == 0
        assert count("after-while-stopped-by-condition-should-not-run") == 0
