"""Shared utilities for ESPHome integration tests - TCP servers."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator, Awaitable, Callable
from contextlib import asynccontextmanager

ConnectionHandler = Callable[
    [asyncio.StreamReader, asyncio.StreamWriter], Awaitable[None]
]


@asynccontextmanager
async def tcp_server(
    handle: ConnectionHandler, port: int, host: str = "127.0.0.1"
) -> AsyncIterator[asyncio.Server]:
    """Serve ``handle`` on ``port``; on exit close every accepted connection, then the server."""
    writers: list[asyncio.StreamWriter] = []

    async def track(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        writers.append(writer)
        await handle(reader, writer)

    server = await asyncio.start_server(track, host, port)
    try:
        yield server
    finally:
        # On Python 3.12 wait_closed() waits for every connection the handlers
        # left open; nothing else closes them once the device has exited.
        for writer in writers:
            writer.close()
        server.close()
        await server.wait_closed()
