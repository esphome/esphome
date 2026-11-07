"""Integration test for the the crc32 helper function."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_crc32_helper(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test the crc32 helper through integration testing."""
    waiter = LineWaiter()

    def passed(name: str) -> bool:
        return any("ALL TESTS PASSED" in line and name in line for line in waiter.lines)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "crc32-helper-test")

        await waiter.wait_for("CRC32 Integration Test Complete", timeout=5.0)

        assert any(
            "CRC32 Helper Integration Test Starting" in line for line in waiter.lines
        ), "CRC32 test setup never started"
        assert passed("Check value"), "Check value test failed"
        assert passed("Known vectors"), "Known vectors test failed"
        assert passed("Chained updates"), "Chained updates test failed"
        assert passed("Empty input"), "Empty input test failed"
