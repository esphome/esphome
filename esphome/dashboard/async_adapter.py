from __future__ import annotations

import asyncio


class AsyncEvent:
    """This is a shim around asyncio.Event."""

    def __init__(self) -> None:
        """Initialize the ThreadedAsyncEvent."""
        self.async_event: asyncio.Event | None = None
        self.loop: asyncio.AbstractEventLoop | None = None

    def async_setup(
        self, loop: asyncio.AbstractEventLoop, async_event: asyncio.Event
    ) -> None:
        """Set the asyncio.Event instance."""
        self.loop = loop
        self.async_event = async_event

    def async_set(self) -> None:
        """Set the asyncio.Event instance."""
        self.async_event.set()

    async def async_wait(self) -> None:
        """Wait the event async."""
        await self.async_event.wait()

    def async_clear(self) -> None:
        """Clear the event async."""
        self.async_event.clear()
