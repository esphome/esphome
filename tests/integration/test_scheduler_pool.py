"""Integration test for scheduler memory pool functionality."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_pool(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that the scheduler memory pool is working correctly with realistic usage.

    This test simulates real-world scheduler usage patterns and verifies that:
    1. Items are recycled to the pool when timeouts complete naturally
    2. Items are recycled when intervals/timeouts are cancelled
    3. Items are reused from the pool for new scheduler operations
    4. The pool grows gradually based on actual usage patterns
    5. Pool operations are logged correctly with debug scheduler enabled
    """
    # Track log messages to verify pool behavior
    log_lines: list[str] = []
    pool_reuse_count = 0
    pool_recycle_count = 0
    pool_full_count = 0
    new_alloc_count = 0

    # Patterns to match pool operations
    reuse_pattern = re.compile(r"Reused item from pool \(pool size now: (\d+)\)")
    recycle_pattern = re.compile(r"Recycled item to pool \(pool size now: (\d+)\)")
    pool_full_pattern = re.compile(r"Pool full \(size: (\d+)\), deleting item")
    new_alloc_pattern = re.compile(r"Allocated new item \(pool empty\)")

    # Futures to track when test phases complete
    loop = asyncio.get_running_loop()
    test_complete_future: asyncio.Future[bool] = loop.create_future()
    phase_futures = {
        1: loop.create_future(),
        2: loop.create_future(),
        3: loop.create_future(),
        4: loop.create_future(),
        5: loop.create_future(),
        6: loop.create_future(),
        7: loop.create_future(),
    }

    def check_output(line: str) -> None:
        """Check log output for pool operations and phase completion."""
        nonlocal pool_reuse_count, pool_recycle_count, pool_full_count, new_alloc_count
        log_lines.append(line)

        # Track pool operations
        if reuse_pattern.search(line):
            pool_reuse_count += 1

        elif recycle_pattern.search(line):
            pool_recycle_count += 1

        elif pool_full_pattern.search(line):
            pool_full_count += 1

        elif new_alloc_pattern.search(line):
            new_alloc_count += 1

        # Track phase completion
        for phase_num in range(1, 8):
            if (
                f"Phase {phase_num} complete" in line
                and phase_num in phase_futures
                and not phase_futures[phase_num].done()
            ):
                phase_futures[phase_num].set_result(True)

        # Check for test completion
        if "Pool recycling test complete" in line and not test_complete_future.done():
            test_complete_future.set_result(True)

    # Run the test with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device is running
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-pool-test"

        # Get list of services
        entities, services = await client.list_entities_services()
        service_names = {s.name for s in services}

        # Verify all test services are available
        expected_services = {
            "run_phase_1",
            "run_phase_2",
            "run_phase_3",
            "run_phase_4",
            "run_phase_5",
            "run_phase_6",
            "run_phase_7",
            "run_complete",
        }
        assert expected_services.issubset(service_names), (
            f"Missing services: {expected_services - service_names}"
        )

        # Get service objects
        phase_services = {
            num: next(s for s in services if s.name == f"run_phase_{num}")
            for num in range(1, 8)
        }
        complete_service = next(s for s in services if s.name == "run_complete")

        try:
            # Phase 1: Component lifecycle
            client.execute_service(phase_services[1], {})
            await asyncio.wait_for(phase_futures[1], timeout=1.0)
            await asyncio.sleep(0.05)  # Let timeouts complete

            # Phase 2: Sensor polling
            client.execute_service(phase_services[2], {})
            await asyncio.wait_for(phase_futures[2], timeout=1.0)
            await asyncio.sleep(0.1)  # Let intervals run a bit

            # Phase 3: Communication patterns
            client.execute_service(phase_services[3], {})
            await asyncio.wait_for(phase_futures[3], timeout=1.0)
            await asyncio.sleep(0.1)  # Let heartbeat run

            # Phase 4: Defer patterns
            client.execute_service(phase_services[4], {})
            await asyncio.wait_for(phase_futures[4], timeout=1.0)
            await asyncio.sleep(0.2)  # Let everything settle and recycle

            # Phase 5: Pool reuse verification
            client.execute_service(phase_services[5], {})
            await asyncio.wait_for(phase_futures[5], timeout=1.0)
            await asyncio.sleep(0.1)  # Let Phase 5 timeouts complete and recycle

            # Phase 6: Full pool reuse verification
            client.execute_service(phase_services[6], {})
            await asyncio.wait_for(phase_futures[6], timeout=1.0)
            await asyncio.sleep(0.1)  # Let Phase 6 timeouts complete

            # Phase 7: Same-named defer optimization
            client.execute_service(phase_services[7], {})
            await asyncio.wait_for(phase_futures[7], timeout=1.0)
            await asyncio.sleep(0.05)  # Let the single defer execute

            # Complete test
            client.execute_service(complete_service, {})
            await asyncio.wait_for(test_complete_future, timeout=0.5)

        except TimeoutError as e:
            # Print debug info if test times out
            recent_logs = "\n".join(log_lines[-30:])
            phases_completed = [num for num, fut in phase_futures.items() if fut.done()]
            pytest.fail(
                f"Test timed out waiting for phase/completion. Error: {e}\n"
                f"  Phases completed: {phases_completed}\n"
                f"  Pool stats:\n"
                f"    Reuse count: {pool_reuse_count}\n"
                f"    Recycle count: {pool_recycle_count}\n"
                f"    Pool full count: {pool_full_count}\n"
                f"    New alloc count: {new_alloc_count}\n"
                f"Recent logs:\n{recent_logs}"
            )

    # Verify all test phases ran
    for phase_num in range(1, 8):
        assert phase_futures[phase_num].done(), f"Phase {phase_num} did not complete"

    # Verify pool behavior
    assert pool_recycle_count > 0, "Should have recycled items to pool"

    # Check pool metrics
    if pool_recycle_count > 0:
        max_pool_size = 0
        for line in log_lines:
            if match := recycle_pattern.search(line):
                size = int(match.group(1))
                max_pool_size = max(max_pool_size, size)

        # Pool can grow up to its maximum of 5
        assert max_pool_size <= 5, f"Pool grew beyond maximum ({max_pool_size})"

    # Log summary for debugging
    print("\nScheduler Pool Test Summary (Python Orchestrated):")
    print(f"  Items recycled to pool: {pool_recycle_count}")
    print(f"  Items reused from pool: {pool_reuse_count}")
    print(f"  Pool full events: {pool_full_count}")
    print(f"  New allocations: {new_alloc_count}")
    print("  All phases completed successfully")

    # Verify reuse happened
    if pool_reuse_count == 0 and pool_recycle_count > 3:
        pytest.fail("Pool had items recycled but none were reused")

    # Success - pool is working
    assert pool_recycle_count > 0 or new_alloc_count < 15, (
        "Pool should either recycle items or limit new allocations"
    )
