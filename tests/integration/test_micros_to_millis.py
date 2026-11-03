"""Integration test for micros_to_millis Euclidean decomposition."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_micros_to_millis(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that micros_to_millis matches reference uint64 division."""

    waiter = LineWaiter()

    def failures() -> list[str]:
        return [line for line in waiter.lines if "FAILED" in line and "[MTM" in line]

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "micros-to-millis-test")

        await wait_or_fail(
            waiter.wait_for("ALL_PASSED"),
            2.0,
            "micros_to_millis test timed out",
            failures=failures,
        )

        assert not failures(), f"micros_to_millis failures: {failures()}"
