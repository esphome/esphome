"""Integration test for vnc password authentication against a fake RFB server.

The right password renders. A wrong one fails exactly once: the client stops retrying,
because TightVNC locks out every user after a few failures, until vnc.connect asks again.
"""

from __future__ import annotations

import asyncio
from pathlib import Path

import pytest

from .rfb_fake_server import HOSTILE_CHUNK, FakeRfbServer
from .test_vnc_client import (
    CONNECTED,
    DISCONNECTED,
    DISPLAY_HEIGHT,
    DISPLAY_WIDTH,
    FIXTURE_PORT,
    Device,
    LogWatch,
    assert_shows,
    exchange,
    full_request,
    next_request,
    paint,
)
from .types import APIClientConnectedFactory, RunCompiledFunction

PASSWORD = "secret12"
WRONG_PASSWORD = "secret13"
# Long enough to cover several 1 s reconnect intervals and their doubling.
NO_RETRY_S = 5.0
SECOND_NO_RETRY_S = 3.0
LATCH_LOG = "Authentication failed. Not retrying"


async def latched_after_one_failure(
    server: FakeRfbServer, log: LogWatch, failures: int, wait_s: float
) -> None:
    """Wait for the next wrong password, then check nothing tries again for ``wait_s``."""
    await server.wait_until(
        lambda: server.auth_failures == failures,
        timeout=10.0,
        what=f"wrong password number {failures}",
    )
    await log.wait_count(LATCH_LOG, failures)
    connections = server.connections
    await asyncio.sleep(wait_s)
    assert server.connections == connections, "retried a wrong password"
    assert server.auth_failures == failures
    assert log.count(LATCH_LOG) == failures


@pytest.mark.asyncio
async def test_vnc_client_auth(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A wrong password is tried once and then only again on vnc.connect."""
    snapshot_dir = tmp_path / "snapshots"
    monkeypatch.setenv("ESPHOME_SNAPSHOT_DIR", str(snapshot_dir))
    log = LogWatch()
    # An empty name and a handshake in 1-7 byte pieces also cover the zero-length and split
    # handshake steps.
    server = FakeRfbServer(
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        password=PASSWORD,
        name="",
        handshake_chunk=HOSTILE_CHUNK,
    )
    paint(server, seed=4)

    async with server:
        assert FIXTURE_PORT in yaml_config
        yaml_config = yaml_config.replace(FIXTURE_PORT, f"port: {server.port}")
        async with (
            run_compiled(yaml_config, line_callback=log.callback),
            api_client_connected() as client,
        ):
            _, services = await client.list_entities_services()
            device = Device(client, services, snapshot_dir)

            # The right password renders.
            request, _ = await next_request(server, timeout=15.0)
            assert request == full_request(incremental=False)
            assert server.client_version == b"RFB 003.008\n"
            assert (server.sessions, server.auth_failures) == (1, 0)
            await log.wait_count(CONNECTED, 1)
            assert log.count("Server '', 64x48, RFB 3.8") == 1
            assert log.count("showing the top-left") == 0
            frame = server.build_update(
                [
                    (0, 0, DISPLAY_WIDTH, 20, "raw"),
                    (0, 20, DISPLAY_WIDTH, DISPLAY_HEIGHT - 20, "hextile"),
                ]
            )
            await exchange(server, frame, log=log)
            assert_shows(await device.snapshot("auth"), server)

            # A wrong password on the next connection fails once, and nothing retries it.
            server.password = WRONG_PASSWORD
            await server.close_client()
            await log.wait_count(DISCONNECTED, 1)
            await latched_after_one_failure(server, log, 1, NO_RETRY_S)
            assert server.connections == 2
            assert log.count("Server: Authentication failed") == 1
            assert (log.count(CONNECTED), log.count(DISCONNECTED)) == (1, 1)

            # vnc.connect tries once more; still wrong, so it latches again.
            await device.run("vnc_connect")
            await latched_after_one_failure(server, log, 2, SECOND_NO_RETRY_S)
            assert server.connections == 3

            # With the password fixed, vnc.connect gets a session again.
            server.password = PASSWORD
            await device.run("vnc_connect")
            await server.wait_until(
                lambda: server.connections == 4,
                timeout=5.0,
                what="a new attempt after vnc.connect",
            )
            await log.wait_count(CONNECTED, 2)
            request, _ = await next_request(server)
            assert request == full_request(incremental=False)
            paint(server, seed=5)
            frame = server.build_update(
                [(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, "hextile")]
            )
            await exchange(server, frame, log=log)
            assert_shows(await device.snapshot("again"), server)
            assert (server.sessions, server.auth_failures) == (2, 2)
            assert log.count(DISCONNECTED) == 1
            assert server.protocol_errors == []
