from __future__ import annotations

import asyncio
import threading


class ThreadedAsyncEvent:
    """This is a shim to allow the asyncio event to be used in a threaded context.

    When more of the code is moved to asyncio, this can be removed.
    """

    def __init__(self) -> None:
        """Initialize the ThreadedAsyncEvent."""
        self.event = threading.Event()
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
        self.event.set()

    def set(self) -> None:
        """Set the event."""
        self.loop.call_soon_threadsafe(self.async_event.set)
        self.event.set()

    def wait(self) -> None:
        """Wait for the event."""
        self.event.wait()

    async def async_wait(self) -> None:
        """Wait the event async."""
        await self.async_event.wait()

    def clear(self) -> None:
        """Clear the event."""
        self.loop.call_soon_threadsafe(self.async_event.clear)
        self.event.clear()

    def async_clear(self) -> None:
        """Clear the event async."""
        self.async_event.clear()
        self.event.clear()

    def is_set(self) -> bool:
        """Return if the event is set."""
        return self.event.is_set()
