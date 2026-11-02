"""Test that if, while and repeat resume the actions after them once their branch finishes."""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_continuation_actions(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """
    Test that if/while/repeat resume the actions after them once their branch finishes.
    """
    waiter = LineWaiter()

    while_iteration_pattern = re.compile(r"while-iteration-(\d+)")
    repeat_iteration_pattern = re.compile(r"repeat-iteration-(\d+)")
    rapid_complete_pattern = re.compile(r"rapid-if-completed: value=(\d+)")

    def iterations(pattern: re.Pattern[str]) -> int:
        """Return the highest logged iteration index plus one."""
        return max(
            (
                int(m.group(1)) + 1
                for line in waiter.lines
                if (m := pattern.search(line))
            ),
            default=0,
        )

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Get services
        _, services = await client.list_entities_services()

        # Test 1: IfAction with then branch
        test_service = require_service(services, "test_if_action")
        if_complete = waiter.expect("if completed")
        await client.execute_service(test_service, {"condition": True, "value": 42})
        await waiter.wait(if_complete, 2.0, "IfAction did not complete")
        assert waiter.count("if-then executed: value="), (
            "IfAction then branch not executed"
        )

        # Test 1b: IfAction with else branch
        if_complete = waiter.expect("if completed")
        await client.execute_service(test_service, {"condition": False, "value": 99})
        await waiter.wait(if_complete, 2.0, "IfAction did not complete")
        assert waiter.count("if-else executed: value="), (
            "IfAction else branch not executed"
        )

        # Test 2: Nested IfAction - test all branches
        test_service = require_service(services, "test_nested_if")

        # Both true
        nested_complete = waiter.expect("nested if completed")
        await client.execute_service(test_service, {"outer": True, "inner": True})
        await waiter.wait(nested_complete, 2.0, "Nested if did not complete")
        assert waiter.count("nested-both-true"), "Nested both true not executed"

        # Outer true, inner false
        nested_complete = waiter.expect("nested if completed")
        await client.execute_service(test_service, {"outer": True, "inner": False})
        await waiter.wait(nested_complete, 2.0, "Nested if did not complete")
        assert waiter.count("nested-outer-true-inner-false"), (
            "Nested outer true inner false not executed"
        )

        # Outer false
        nested_complete = waiter.expect("nested if completed")
        await client.execute_service(test_service, {"outer": False, "inner": True})
        await waiter.wait(nested_complete, 2.0, "Nested if did not complete")
        assert waiter.count("nested-outer-false"), "Nested outer false not executed"

        # Test 3: WhileAction
        test_service = require_service(services, "test_while_action")
        while_complete = waiter.expect("while completed")
        await client.execute_service(test_service, {"max_count": 3})
        await waiter.wait(while_complete, 2.0, "WhileAction did not complete")
        while_iterations = iterations(while_iteration_pattern)
        assert while_iterations == 3, (
            f"WhileAction expected 3 iterations, got {while_iterations}"
        )

        # Test 4: RepeatAction
        test_service = require_service(services, "test_repeat_action")
        repeat_complete = waiter.expect("repeat completed")
        await client.execute_service(test_service, {"count": 5})
        await waiter.wait(repeat_complete, 2.0, "RepeatAction did not complete")
        repeat_iterations = iterations(repeat_iteration_pattern)
        assert repeat_iterations == 5, (
            f"RepeatAction expected 5 iterations, got {repeat_iterations}"
        )

        # Test 5: Combined (if + repeat + while)
        test_service = require_service(services, "test_combined")
        combined_complete = waiter.expect("combined completed")
        await client.execute_service(test_service, {"do_loop": True, "loop_count": 2})
        await waiter.wait(combined_complete, 2.0, "Combined did not complete")
        # Should execute: repeat 2 times, each iteration does while from iteration down to 0
        # iteration 0: while 0 times = 0
        # iteration 1: while 1 time = 1
        # Total: 1 combined log
        combined_iterations = waiter.count(re.compile(r"combined-repeat\d+-while\d+"))
        assert combined_iterations >= 1, (
            f"Combined expected >=1 iterations, got {combined_iterations}"
        )

        # Test 6: Rapid triggers through an if/else
        test_service = require_service(services, "test_rapid_if")
        await client.execute_service(test_service, {})
        await waiter.wait_count(
            rapid_complete_pattern, 5, 2.0, "Rapid test did not complete 5 times"
        )
        # Values 1, 2 should hit else (<=2), values 3, 4, 5 should hit then (>2)
        rapid_else = waiter.count("rapid-if-else: value=")
        assert rapid_else == 2, f"Rapid test expected 2 else, got {rapid_else}"
        rapid_then = waiter.count("rapid-if-then: value=")
        assert rapid_then == 3, f"Rapid test expected 3 then, got {rapid_then}"
        rapid_complete = waiter.count(rapid_complete_pattern)
        assert rapid_complete == 5, (
            f"Rapid test expected 5 completions, got {rapid_complete}"
        )
