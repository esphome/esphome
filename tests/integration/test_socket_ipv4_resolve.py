"""Integration test for the socket Ipv4Resolve helper on host."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

CHECKS = (
    "Literal resolve",
    "Forget drops address",
    "IPv6 literal rejected",
    "Hostname resolve",
)


@pytest.mark.asyncio
async def test_socket_ipv4_resolve(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Exercise Ipv4Resolve literals, forget, failure, and getaddrinfo on host."""
    waiter = LineWaiter()

    def passed(check: str) -> bool:
        # The last report of a check wins
        reports = [
            line
            for line in waiter.lines
            if f"{check}:" in line and "IPv4 resolve test complete" not in line
        ]
        return bool(reports) and "PASSED" in reports[-1]

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "socket-ipv4-resolve-test")

        # The outer bound carries the message; the inner one only has to outlast it
        await wait_or_fail(
            waiter.wait_for("IPv4 resolve test complete", timeout=20.0),
            10.0,
            "IPv4 resolve test timed out",
        )

    for check in CHECKS:
        assert passed(check), f"{check} check failed or never ran"
