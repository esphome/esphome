"""Test ESPHome queued script functionality."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_script_queued(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test comprehensive queued script functionality."""
    loop = asyncio.get_running_loop()

    # Track all test results
    test_results = {
        "queue_depth": {"processed": [], "rejections": 0},
        "ring_buffer": {"start_order": [], "end_order": []},
        "stop": {"processed": [], "stop_logged": False},
        "rejection": {"processed": [], "rejections": 0},
        "no_params": {"executions": 0},
    }

    # Patterns for Test 1: Queue depth
    queue_start = re.compile(r"Queue test: START item (\d+)")
    queue_end = re.compile(r"Queue test: END item (\d+)")
    queue_reject = re.compile(r"Script 'queue_depth_script' max instances")

    # Patterns for Test 2: Ring buffer
    ring_start = re.compile(r"Ring buffer: START '([A-Z])'")
    ring_end = re.compile(r"Ring buffer: END '([A-Z])'")

    # Patterns for Test 3: Stop
    stop_start = re.compile(r"Stop test: START (\d+)")
    stop_log = re.compile(r"STOPPING script now")

    # Patterns for Test 4: Rejection
    reject_start = re.compile(r"Rejection test: START (\d+)")
    reject_end = re.compile(r"Rejection test: END (\d+)")
    reject_reject = re.compile(r"Script 'rejection_script' max instances")

    # Patterns for Test 5: No params
    no_params_end = re.compile(r"No params: END")

    # Test completion futures
    test1_complete = loop.create_future()
    test2_complete = loop.create_future()
    test3_complete = loop.create_future()
    test4_complete = loop.create_future()
    test5_complete = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for all test messages."""
        # Test 1: Queue depth
        if match := queue_start.search(line):
            item = int(match.group(1))
            if item not in test_results["queue_depth"]["processed"]:
                test_results["queue_depth"]["processed"].append(item)

        if match := queue_end.search(line):
            item = int(match.group(1))
            if item == 5 and not test1_complete.done():
                test1_complete.set_result(True)

        if queue_reject.search(line):
            test_results["queue_depth"]["rejections"] += 1

        # Test 2: Ring buffer
        if match := ring_start.search(line):
            msg = match.group(1)
            test_results["ring_buffer"]["start_order"].append(msg)

        if match := ring_end.search(line):
            msg = match.group(1)
            test_results["ring_buffer"]["end_order"].append(msg)
            if (
                len(test_results["ring_buffer"]["end_order"]) == 3
                and not test2_complete.done()
            ):
                test2_complete.set_result(True)

        # Test 3: Stop
        if match := stop_start.search(line):
            item = int(match.group(1))
            if item not in test_results["stop"]["processed"]:
                test_results["stop"]["processed"].append(item)

        if stop_log.search(line):
            test_results["stop"]["stop_logged"] = True
            # Give time for any queued items to be cleared
            if not test3_complete.done():
                loop.call_later(
                    0.3,
                    lambda: test3_complete.set_result(True)
                    if not test3_complete.done()
                    else None,
                )

        # Test 4: Rejection
        if match := reject_start.search(line):
            item = int(match.group(1))
            if item not in test_results["rejection"]["processed"]:
                test_results["rejection"]["processed"].append(item)

        if match := reject_end.search(line):
            item = int(match.group(1))
            if item == 3 and not test4_complete.done():
                test4_complete.set_result(True)

        if reject_reject.search(line):
            test_results["rejection"]["rejections"] += 1

        # Test 5: No params
        if no_params_end.search(line):
            test_results["no_params"]["executions"] += 1
            if (
                test_results["no_params"]["executions"] == 3
                and not test5_complete.done()
            ):
                test5_complete.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Get services
        _, services = await client.list_entities_services()

        # Test 1: Queue depth limit
        test_service = next((s for s in services if s.name == "test_queue_depth"), None)
        assert test_service is not None, "test_queue_depth service not found"
        client.execute_service(test_service, {})
        await asyncio.wait_for(test1_complete, timeout=2.0)
        await asyncio.sleep(0.1)  # Give time for rejections

        # Verify Test 1
        assert sorted(test_results["queue_depth"]["processed"]) == [1, 2, 3, 4, 5], (
            f"Test 1: Expected to process items 1-5 (max_runs=5 means 5 total), got {sorted(test_results['queue_depth']['processed'])}"
        )
        assert test_results["queue_depth"]["rejections"] >= 2, (
            "Test 1: Expected at least 2 rejection warnings (items 6-7 should be rejected)"
        )

        # Test 2: Ring buffer order
        test_service = next((s for s in services if s.name == "test_ring_buffer"), None)
        assert test_service is not None, "test_ring_buffer service not found"
        client.execute_service(test_service, {})
        await asyncio.wait_for(test2_complete, timeout=2.0)

        # Verify Test 2
        assert test_results["ring_buffer"]["start_order"] == ["A", "B", "C"], (
            f"Test 2: Expected start order [A, B, C], got {test_results['ring_buffer']['start_order']}"
        )
        assert test_results["ring_buffer"]["end_order"] == ["A", "B", "C"], (
            f"Test 2: Expected end order [A, B, C], got {test_results['ring_buffer']['end_order']}"
        )

        # Test 3: Stop clears queue
        test_service = next((s for s in services if s.name == "test_stop_clears"), None)
        assert test_service is not None, "test_stop_clears service not found"
        client.execute_service(test_service, {})
        await asyncio.wait_for(test3_complete, timeout=2.0)

        # Verify Test 3
        assert test_results["stop"]["stop_logged"], (
            "Test 3: Stop command was not logged"
        )
        assert test_results["stop"]["processed"] == [1], (
            f"Test 3: Expected only item 1 to process, got {test_results['stop']['processed']}"
        )

        # Test 4: Rejection enforcement (max_runs=3)
        test_service = next((s for s in services if s.name == "test_rejection"), None)
        assert test_service is not None, "test_rejection service not found"
        client.execute_service(test_service, {})
        await asyncio.wait_for(test4_complete, timeout=2.0)
        await asyncio.sleep(0.1)  # Give time for rejections

        # Verify Test 4
        assert sorted(test_results["rejection"]["processed"]) == [1, 2, 3], (
            f"Test 4: Expected to process items 1-3 (max_runs=3 means 3 total), got {sorted(test_results['rejection']['processed'])}"
        )
        assert test_results["rejection"]["rejections"] == 5, (
            f"Test 4: Expected 5 rejections (items 4-8), got {test_results['rejection']['rejections']}"
        )

        # Test 5: No parameters
        test_service = next((s for s in services if s.name == "test_no_params"), None)
        assert test_service is not None, "test_no_params service not found"
        client.execute_service(test_service, {})
        await asyncio.wait_for(test5_complete, timeout=2.0)

        # Verify Test 5
        assert test_results["no_params"]["executions"] == 3, (
            f"Test 5: Expected 3 executions, got {test_results['no_params']['executions']}"
        )
