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

        # Automations that run directly from the callback get their arguments, in order,
        # and the on and off filters still apply.
        # Conditions, branches and loops with inline bodies choose the same paths
        for value, branch, choice in (
            (5, "then", "small"),
            (50, "then", "medium"),
            (500, "else", "large"),
            (0, "else", "small"),
        ):
            start = len(waiter.lines)
            await client.execute_service(service["run_control_flow"], {"value": value})
            await waiter.wait_for(f"FLOW done {value}", timeout=5.0)
            flow = [
                match.group(1)
                for line in waiter.lines[start:]
                if (match := re.search(r"FLOW ([a-z ]+[\d ]*\d)", line))
            ]
            repeats = [
                f"repeat {outer} {inner} {value}"
                for outer in (0, 1)
                for inner in (0, 1)
            ]
            assert flow == [
                f"if {branch} {value}",
                f"choose {choice} {value}",
                *(repeats if value > 0 else []),
                "while 1",
                "while 2",
                "while 3",
                f"done {value}",
            ]

        # A stop in a loop body or in a condition ends the script at that point
        await client.execute_service(service["run_stop_in_loops"], {})
        await waiter.wait_for("LOOP_STOP done", timeout=5.0)
        assert count("LOOP_STOP repeat 0") == 1
        assert count("LOOP_STOP repeat 1") == 1
        assert count("LOOP_STOP repeat 2") == 0
        assert count("LOOP_STOP repeat after") == 0
        assert count("LOOP_STOP while 1") == 1
        assert count("LOOP_STOP while 2") == 1
        assert count("LOOP_STOP while 3") == 0
        assert count("LOOP_STOP while after") == 0
        assert count("LOOP_STOP condition then") == 0
        assert count("LOOP_STOP condition after") == 0

        # A nested run whose while condition is false does not end the outer run. WhileAction
        # stopped its body there, which lost the outer run's lines and left it counted as running.
        await client.execute_service(service["run_while_reentry"], {})
        await waiter.wait_for("WHILE_REENTER running", timeout=5.0)
        assert count("WHILE_REENTER after nested") == 1
        assert count("WHILE_REENTER after while") == 2
        assert count("WHILE_REENTER running 0") == 1

        # The switch also publishes its state at boot, so only later lines count.
        boot_lines = len(waiter.lines)
        await client.execute_service(service["run_direct"], {})
        await waiter.wait_for("DIRECT done off", timeout=5.0)
        direct = [
            match.group(1)
            for line in waiter.lines[boot_lines:]
            if (match := re.search(r"DIRECT ((?:state|on|off|done) \w+(?: \d)?)", line))
        ]
        assert sorted(direct) == sorted(
            [
                "state first 1",
                "state second 1",
                "on first",
                "on second",
                "done on",
                "state first 0",
                "state second 0",
                "off first",
                "off second",
                "done off",
            ]
        )
        assert direct.index("on first") < direct.index("on second")
        assert direct.index("off first") < direct.index("off second")
        assert direct.index("state first 1") < direct.index("state second 1")
        assert direct.index("done on") < direct.index("off first")

        # script.execute evaluates its arguments left to right
        await client.execute_service(service["run_arg_order"], {})
        await waiter.wait_for("ARG_ORDER done", timeout=5.0)
        order = [
            match.group(1)
            for line in waiter.lines
            if (match := re.search(r"ARG_ORDER (eval \w+|run \d \d)", line))
        ]
        assert order == ["eval first", "eval second", "run 1 2"]

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
