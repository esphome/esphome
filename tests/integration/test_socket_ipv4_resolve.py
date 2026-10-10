"""Integration test for the socket Ipv4Resolve helper on host."""

from __future__ import annotations

import asyncio

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

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
    test_complete = asyncio.Event()
    results: dict[str, bool] = {}

    def on_log_line(line: str) -> None:
        if "IPv4 resolve test complete" in line:
            test_complete.set()
            return
        for check in CHECKS:
            if f"{check}:" in line:
                results[check] = "PASSED" in line

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "socket-ipv4-resolve-test"

        try:
            await asyncio.wait_for(test_complete.wait(), timeout=10.0)
        except TimeoutError:
            pytest.fail("IPv4 resolve test timed out")

    for check in CHECKS:
        assert results.get(check), f"{check} check failed or never ran"
