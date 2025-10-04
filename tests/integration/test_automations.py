"""Test ESPHome automations functionality."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_delay_action_cancellation(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that delay actions can be properly cancelled when script restarts."""
    loop = asyncio.get_running_loop()

    # Track log messages with timestamps
    log_entries: list[tuple[float, str]] = []
    script_starts: list[float] = []
    delay_completions: list[float] = []
    script_restart_logged = False
    test_started_time = None

    # Patterns to match
    test_start_pattern = re.compile(r"Starting first script execution")
    script_start_pattern = re.compile(r"Script started, beginning delay")
    restart_pattern = re.compile(r"Restarting script \(should cancel first delay\)")
    delay_complete_pattern = re.compile(r"Delay completed successfully")

    # Future to track when we can check results
    second_script_started = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        nonlocal script_restart_logged, test_started_time

        current_time = loop.time()
        log_entries.append((current_time, line))

        if test_start_pattern.search(line):
            test_started_time = current_time
        elif script_start_pattern.search(line) and test_started_time:
            script_starts.append(current_time)
            if len(script_starts) == 2 and not second_script_started.done():
                second_script_started.set_result(True)
        elif restart_pattern.search(line):
            script_restart_logged = True
        elif delay_complete_pattern.search(line):
            delay_completions.append(current_time)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Get services
        entities, services = await client.list_entities_services()

        # Find our test service
        test_service = next(
            (s for s in services if s.name == "start_delay_then_restart"), None
        )
        assert test_service is not None, "start_delay_then_restart service not found"

        # Execute the test sequence
        client.execute_service(test_service, {})

        # Wait for the second script to start
        await asyncio.wait_for(second_script_started, timeout=5.0)

        # Wait for potential delay completion
        await asyncio.sleep(0.75)  # Original delay was 500ms

        # Check results
        assert len(script_starts) == 2, (
            f"Script should have started twice, but started {len(script_starts)} times"
        )
        assert script_restart_logged, "Script restart was not logged"

        # Verify we got exactly one completion and it happened ~500ms after the second start
        assert len(delay_completions) == 1, (
            f"Expected 1 delay completion, got {len(delay_completions)}"
        )
        time_from_second_start = delay_completions[0] - script_starts[1]
        assert 0.4 < time_from_second_start < 0.6, (
            f"Delay completed {time_from_second_start:.3f}s after second start, expected ~0.5s"
        )


@pytest.mark.asyncio
async def test_parallel_script_delays(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that parallel scripts with delays don't interfere with each other."""
    loop = asyncio.get_running_loop()

    # Track script executions
    script_starts: list[float] = []
    script_ends: list[float] = []

    # Patterns to match
    start_pattern = re.compile(r"Parallel script instance \d+ started")
    end_pattern = re.compile(r"Parallel script instance \d+ completed after delay")

    # Future to track when all scripts have completed
    all_scripts_completed = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for parallel script messages."""
        current_time = loop.time()

        if start_pattern.search(line):
            script_starts.append(current_time)

        if end_pattern.search(line):
            script_ends.append(current_time)
            # Check if we have all 3 completions
            if len(script_ends) == 3 and not all_scripts_completed.done():
                all_scripts_completed.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Get services
        entities, services = await client.list_entities_services()

        # Find our test service
        test_service = next(
            (s for s in services if s.name == "test_parallel_delays"), None
        )
        assert test_service is not None, "test_parallel_delays service not found"

        # Execute the test - this will start 3 parallel scripts with 1 second delays
        client.execute_service(test_service, {})

        # Wait for all scripts to complete (should take ~1 second, not 3)
        await asyncio.wait_for(all_scripts_completed, timeout=2.0)

        # Verify we had 3 starts and 3 ends
        assert len(script_starts) == 3, (
            f"Expected 3 script starts, got {len(script_starts)}"
        )
        assert len(script_ends) == 3, f"Expected 3 script ends, got {len(script_ends)}"

        # Verify they ran in parallel - all should complete within ~1.5 seconds
        first_start = min(script_starts)
        last_end = max(script_ends)
        total_time = last_end - first_start

        # If running in parallel, total time should be close to 1 second
        # If they were interfering (running sequentially), it would take 3+ seconds
        assert total_time < 1.5, (
            f"Parallel scripts took {total_time:.2f}s total, should be ~1s if running in parallel"
        )
