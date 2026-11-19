"""Integration test for scheduler memory pool functionality."""

from __future__ import annotations

import asyncio
import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


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
    # Patterns to match pool operations
    reuse_pattern = re.compile(r"Reused item from pool \(pool size now: (\d+)\)")
    recycle_pattern = re.compile(r"Recycled item to pool \(pool size now: (\d+)\)")
    pool_full_pattern = re.compile(r"Pool full \(size: (\d+)\), deleting item")
    new_alloc_pattern = re.compile(r"Allocated new item \(pool empty\)")

    # Futures to track when test phases complete
    waiter = LineWaiter()
    test_complete_future = waiter.expect("Pool recycling test complete")
    phase_futures = {
        phase_num: waiter.expect(f"Phase {phase_num} complete")
        for phase_num in range(1, 8)
    }

    # Run the test with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device is running
        await assert_device_name(client, "scheduler-pool-test")

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
            num: require_service(services, f"run_phase_{num}") for num in range(1, 8)
        }
        complete_service = require_service(services, "run_complete")

        # Shown when a phase does not complete
        pool = {
            "phases_completed": lambda: [
                n for n, fut in phase_futures.items() if fut.done()
            ],
            "reuse": lambda: waiter.count(reuse_pattern),
            "recycle": lambda: waiter.count(recycle_pattern),
            "pool_full": lambda: waiter.count(pool_full_pattern),
            "new_alloc": lambda: waiter.count(new_alloc_pattern),
            "logged": waiter.lines,
        }
        # Phase 1: Component lifecycle
        await client.execute_service(phase_services[1], {})
        await wait_or_fail(phase_futures[1], 1.0, "Phase 1 did not complete", **pool)
        await asyncio.sleep(0.05)  # Let timeouts complete

        # Phase 2: Sensor polling
        await client.execute_service(phase_services[2], {})
        await wait_or_fail(phase_futures[2], 1.0, "Phase 2 did not complete", **pool)
        await asyncio.sleep(0.1)  # Let intervals run a bit

        # Phase 3: Communication patterns
        await client.execute_service(phase_services[3], {})
        await wait_or_fail(phase_futures[3], 1.0, "Phase 3 did not complete", **pool)
        await asyncio.sleep(0.1)  # Let heartbeat run

        # Phase 4: Defer patterns
        await client.execute_service(phase_services[4], {})
        await wait_or_fail(phase_futures[4], 1.0, "Phase 4 did not complete", **pool)
        await asyncio.sleep(0.2)  # Let everything settle and recycle

        # Phase 5: Pool reuse verification
        await client.execute_service(phase_services[5], {})
        await wait_or_fail(phase_futures[5], 1.0, "Phase 5 did not complete", **pool)
        await asyncio.sleep(0.1)  # Let Phase 5 timeouts complete and recycle

        # Phase 6: Full pool reuse verification
        await client.execute_service(phase_services[6], {})
        await wait_or_fail(phase_futures[6], 1.0, "Phase 6 did not complete", **pool)
        await asyncio.sleep(0.1)  # Let Phase 6 timeouts complete

        # Phase 7: Same-named defer optimization
        await client.execute_service(phase_services[7], {})
        await wait_or_fail(phase_futures[7], 1.0, "Phase 7 did not complete", **pool)
        await asyncio.sleep(0.05)  # Let the single defer execute

        # Complete test
        await client.execute_service(complete_service, {})
        await wait_or_fail(test_complete_future, 0.5, "Test did not complete", **pool)

    # Verify all test phases ran
    for phase_num in range(1, 8):
        assert phase_futures[phase_num].done(), f"Phase {phase_num} did not complete"

    pool_reuse_count = waiter.count(reuse_pattern)
    pool_recycle_count = waiter.count(recycle_pattern)
    pool_full_count = waiter.count(pool_full_pattern)
    new_alloc_count = waiter.count(new_alloc_pattern)

    # Verify pool behavior
    assert pool_recycle_count > 0, "Should have recycled items to pool"

    # Pool is unbounded; the cap was the source of the churn it was meant to prevent.
    assert pool_full_count == 0, (
        f"Pool should never report full (got {pool_full_count})"
    )

    # Verify the pool actually grew past the old MAX_POOL_SIZE=5 cap.
    # Phase 5 + Phase 6 schedule 8 + 10 same-component timeouts respectively, so the
    # observed peak should comfortably exceed 5. Without this lower-bound check, a
    # silent regression that re-introduced a small cap could pass the test above.
    max_pool_size = 0
    for line in waiter.lines:
        if match := recycle_pattern.search(line):
            max_pool_size = max(max_pool_size, int(match.group(1)))
    assert max_pool_size > 5, (
        f"Pool should grow past the old cap of 5; observed peak {max_pool_size}"
    )

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
