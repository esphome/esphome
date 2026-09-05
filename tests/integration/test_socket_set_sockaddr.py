"""Integration test for the socket::set_sockaddr failure contract."""

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_socket_set_sockaddr(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """set_sockaddr reports an invalid address with 0 and accepts broadcast."""
    loop = asyncio.get_running_loop()
    result: asyncio.Future[tuple[int, int, int]] = loop.create_future()

    def on_log_line(line: str) -> None:
        match = re.search(
            r"SET_SOCKADDR invalid=(\d+) valid=(\d+) broadcast=(\d+)", line
        )
        if match and not result.done():
            result.set_result(tuple(int(g) for g in match.groups()))

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        assert (await client.device_info()).name == "socket-set-sockaddr"
        try:
            invalid, valid, broadcast = await asyncio.wait_for(result, timeout=10.0)
        except TimeoutError:
            pytest.fail("SET_SOCKADDR marker never appeared")

    assert invalid == 0
    assert valid > 0
    assert broadcast == valid
