"""Integration test for CRC8 helper function."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_crc8_helper(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test the CRC8 helper function through integration testing."""
    waiter = LineWaiter()

    def passed(name: str) -> bool:
        return any("ALL TESTS PASSED" in line and name in line for line in waiter.lines)

    # Compile and run the test
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "crc8-helper-test")

        # Wait for tests to complete with timeout
        await waiter.wait_for("CRC8 Integration Test Complete", timeout=5.0)

        # Verify all tests passed
        assert any(
            "CRC8 Helper Function Integration Test Starting" in line
            for line in waiter.lines
        ), "CRC8 test setup never started"
        assert passed("Dallas/Maxim CRC8"), "Dallas/Maxim CRC8 test failed"
        assert passed("Sensirion CRC8"), "Sensirion CRC8 test failed"
        assert passed("PEC CRC8"), "PEC CRC8 test failed"
        assert passed("Parameter equivalence"), "Parameter equivalence test failed"
        assert passed("Edge cases"), "Edge cases test failed"
        assert passed("Component compatibility"), "Component compatibility test failed"
