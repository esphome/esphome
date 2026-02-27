"""Test continuation actions (ContinuationAction, WhileLoopContinuation, RepeatLoopContinuation)."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_continuation_actions(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """
    Test that continuation actions work correctly for if/while/repeat.

    These continuation classes replace LambdaAction with simple parent pointers,
    saving 32-36 bytes per instance and eliminating std::function overhead.
    """
    loop = asyncio.get_running_loop()

    # Track test completions
    test_results = {
        "if_then": False,
        "if_else": False,
        "if_complete": False,
        "nested_both_true": False,
        "nested_outer_true_inner_false": False,
        "nested_outer_false": False,
        "nested_complete": False,
        "while_iterations": 0,
        "while_complete": False,
        "repeat_iterations": 0,
        "repeat_complete": False,
        "combined_iterations": 0,
        "combined_complete": False,
        "rapid_then": 0,
        "rapid_else": 0,
        "rapid_complete": 0,
    }

    # Patterns for log messages
    if_then_pattern = re.compile(r"if-then executed: value=(\d+)")
    if_else_pattern = re.compile(r"if-else executed: value=(\d+)")
    if_complete_pattern = re.compile(r"if completed")
    nested_both_true_pattern = re.compile(r"nested-both-true")
    nested_outer_true_inner_false_pattern = re.compile(r"nested-outer-true-inner-false")
    nested_outer_false_pattern = re.compile(r"nested-outer-false")
    nested_complete_pattern = re.compile(r"nested if completed")
    while_iteration_pattern = re.compile(r"while-iteration-(\d+)")
    while_complete_pattern = re.compile(r"while completed")
    repeat_iteration_pattern = re.compile(r"repeat-iteration-(\d+)")
    repeat_complete_pattern = re.compile(r"repeat completed")
    combined_pattern = re.compile(r"combined-repeat(\d+)-while(\d+)")
    combined_complete_pattern = re.compile(r"combined completed")
    rapid_then_pattern = re.compile(r"rapid-if-then: value=(\d+)")
    rapid_else_pattern = re.compile(r"rapid-if-else: value=(\d+)")
    rapid_complete_pattern = re.compile(r"rapid-if-completed: value=(\d+)")

    # Test completion futures
    test1_complete = loop.create_future()  # if action
    test2_complete = loop.create_future()  # nested if
    test3_complete = loop.create_future()  # while
    test4_complete = loop.create_future()  # repeat
    test5_complete = loop.create_future()  # combined
    test6_complete = loop.create_future()  # rapid

    def check_output(line: str) -> None:
        """Check log output for test messages."""
        # Test 1: IfAction
        if if_then_pattern.search(line):
            test_results["if_then"] = True
        if if_else_pattern.search(line):
            test_results["if_else"] = True
        if if_complete_pattern.search(line):
            test_results["if_complete"] = True
            if not test1_complete.done():
                test1_complete.set_result(True)

        # Test 2: Nested IfAction
        if nested_both_true_pattern.search(line):
            test_results["nested_both_true"] = True
        if nested_outer_true_inner_false_pattern.search(line):
            test_results["nested_outer_true_inner_false"] = True
        if nested_outer_false_pattern.search(line):
            test_results["nested_outer_false"] = True
        if nested_complete_pattern.search(line):
            test_results["nested_complete"] = True
            if not test2_complete.done():
                test2_complete.set_result(True)

        # Test 3: WhileAction
        if match := while_iteration_pattern.search(line):
            test_results["while_iterations"] = max(
                test_results["while_iterations"], int(match.group(1)) + 1
            )
        if while_complete_pattern.search(line):
            test_results["while_complete"] = True
            if not test3_complete.done():
                test3_complete.set_result(True)

        # Test 4: RepeatAction
        if match := repeat_iteration_pattern.search(line):
            test_results["repeat_iterations"] = max(
                test_results["repeat_iterations"], int(match.group(1)) + 1
            )
        if repeat_complete_pattern.search(line):
            test_results["repeat_complete"] = True
            if not test4_complete.done():
                test4_complete.set_result(True)

        # Test 5: Combined
        if combined_pattern.search(line):
            test_results["combined_iterations"] += 1
        if combined_complete_pattern.search(line):
            test_results["combined_complete"] = True
            if not test5_complete.done():
                test5_complete.set_result(True)

        # Test 6: Rapid triggers
        if rapid_then_pattern.search(line):
            test_results["rapid_then"] += 1
        if rapid_else_pattern.search(line):
            test_results["rapid_else"] += 1
        if rapid_complete_pattern.search(line):
            test_results["rapid_complete"] += 1
            if test_results["rapid_complete"] == 5 and not test6_complete.done():
                test6_complete.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Get services
        _, services = await client.list_entities_services()

        # Test 1: IfAction with then branch
        test_service = next((s for s in services if s.name == "test_if_action"), None)
        assert test_service is not None, "test_if_action service not found"
        await client.execute_service(test_service, {"condition": True, "value": 42})
        await asyncio.wait_for(test1_complete, timeout=2.0)
        assert test_results["if_then"], "IfAction then branch not executed"
        assert test_results["if_complete"], "IfAction did not complete"

        # Test 1b: IfAction with else branch
        test1_complete = loop.create_future()
        test_results["if_complete"] = False
        await client.execute_service(test_service, {"condition": False, "value": 99})
        await asyncio.wait_for(test1_complete, timeout=2.0)
        assert test_results["if_else"], "IfAction else branch not executed"
        assert test_results["if_complete"], "IfAction did not complete"

        # Test 2: Nested IfAction - test all branches
        test_service = next((s for s in services if s.name == "test_nested_if"), None)
        assert test_service is not None, "test_nested_if service not found"

        # Both true
        await client.execute_service(test_service, {"outer": True, "inner": True})
        await asyncio.wait_for(test2_complete, timeout=2.0)
        assert test_results["nested_both_true"], "Nested both true not executed"

        # Outer true, inner false
        test2_complete = loop.create_future()
        test_results["nested_complete"] = False
        await client.execute_service(test_service, {"outer": True, "inner": False})
        await asyncio.wait_for(test2_complete, timeout=2.0)
        assert test_results["nested_outer_true_inner_false"], (
            "Nested outer true inner false not executed"
        )

        # Outer false
        test2_complete = loop.create_future()
        test_results["nested_complete"] = False
        await client.execute_service(test_service, {"outer": False, "inner": True})
        await asyncio.wait_for(test2_complete, timeout=2.0)
        assert test_results["nested_outer_false"], "Nested outer false not executed"

        # Test 3: WhileAction
        test_service = next(
            (s for s in services if s.name == "test_while_action"), None
        )
        assert test_service is not None, "test_while_action service not found"
        await client.execute_service(test_service, {"max_count": 3})
        await asyncio.wait_for(test3_complete, timeout=2.0)
        assert test_results["while_iterations"] == 3, (
            f"WhileAction expected 3 iterations, got {test_results['while_iterations']}"
        )
        assert test_results["while_complete"], "WhileAction did not complete"

        # Test 4: RepeatAction
        test_service = next(
            (s for s in services if s.name == "test_repeat_action"), None
        )
        assert test_service is not None, "test_repeat_action service not found"
        await client.execute_service(test_service, {"count": 5})
        await asyncio.wait_for(test4_complete, timeout=2.0)
        assert test_results["repeat_iterations"] == 5, (
            f"RepeatAction expected 5 iterations, got {test_results['repeat_iterations']}"
        )
        assert test_results["repeat_complete"], "RepeatAction did not complete"

        # Test 5: Combined (if + repeat + while)
        test_service = next((s for s in services if s.name == "test_combined"), None)
        assert test_service is not None, "test_combined service not found"
        await client.execute_service(test_service, {"do_loop": True, "loop_count": 2})
        await asyncio.wait_for(test5_complete, timeout=2.0)
        # Should execute: repeat 2 times, each iteration does while from iteration down to 0
        # iteration 0: while 0 times = 0
        # iteration 1: while 1 time = 1
        # Total: 1 combined log
        assert test_results["combined_iterations"] >= 1, (
            f"Combined expected >=1 iterations, got {test_results['combined_iterations']}"
        )
        assert test_results["combined_complete"], "Combined did not complete"

        # Test 6: Rapid triggers (tests memory efficiency of ContinuationAction)
        test_service = next((s for s in services if s.name == "test_rapid_if"), None)
        assert test_service is not None, "test_rapid_if service not found"
        await client.execute_service(test_service, {})
        await asyncio.wait_for(test6_complete, timeout=2.0)
        # Values 1, 2 should hit else (<=2), values 3, 4, 5 should hit then (>2)
        assert test_results["rapid_else"] == 2, (
            f"Rapid test expected 2 else, got {test_results['rapid_else']}"
        )
        assert test_results["rapid_then"] == 3, (
            f"Rapid test expected 3 then, got {test_results['rapid_then']}"
        )
        assert test_results["rapid_complete"] == 5, (
            f"Rapid test expected 5 completions, got {test_results['rapid_complete']}"
        )
