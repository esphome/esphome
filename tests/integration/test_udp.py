"""Integration test for UDP component."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncGenerator
import contextlib
from contextlib import asynccontextmanager
from dataclasses import dataclass, field
import socket

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@dataclass
class UDPReceiver:
    """Collects UDP messages received."""

    messages: list[bytes] = field(default_factory=list)
    message_received: asyncio.Event = field(default_factory=asyncio.Event)

    def on_message(self, data: bytes) -> None:
        """Called when a message is received."""
        self.messages.append(data)
        self.message_received.set()

    async def wait_for_message(self, timeout: float = 5.0) -> bytes:
        """Wait for a message to be received."""
        await asyncio.wait_for(self.message_received.wait(), timeout=timeout)
        return self.messages[-1]

    async def wait_for_content(self, content: bytes, timeout: float = 5.0) -> bytes:
        """Wait for a specific message content."""
        deadline = asyncio.get_event_loop().time() + timeout
        while True:
            for msg in self.messages:
                if content in msg:
                    return msg
            remaining = deadline - asyncio.get_event_loop().time()
            if remaining <= 0:
                raise TimeoutError(
                    f"Content {content!r} not found in messages: {self.messages}"
                )
            try:
                await asyncio.wait_for(self.message_received.wait(), timeout=remaining)
                self.message_received.clear()
            except TimeoutError:
                raise TimeoutError(
                    f"Content {content!r} not found in messages: {self.messages}"
                ) from None


@asynccontextmanager
async def udp_listener(port: int = 0) -> AsyncGenerator[tuple[int, UDPReceiver]]:
    """Async context manager that listens for UDP messages.

    Args:
        port: Port to listen on. 0 for auto-assign.

    Yields:
        Tuple of (port, UDPReceiver) where port is the UDP port being listened on.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("127.0.0.1", port))
    sock.setblocking(False)
    actual_port = sock.getsockname()[1]

    receiver = UDPReceiver()

    async def receive_messages() -> None:
        """Background task to receive UDP messages."""
        loop = asyncio.get_running_loop()
        while True:
            try:
                data = await loop.sock_recv(sock, 4096)
                if data:
                    receiver.on_message(data)
            except BlockingIOError:
                await asyncio.sleep(0.01)
            except Exception:
                break

    task = asyncio.create_task(receive_messages())
    try:
        yield actual_port, receiver
    finally:
        task.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await task
        sock.close()


def _get_free_udp_port() -> int:
    """Get a free UDP port by binding to port 0 and releasing."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


@pytest.mark.asyncio
async def test_udp_send_receive(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test UDP component can send and receive messages."""
    log_lines: list[str] = []
    receive_event = asyncio.Event()

    def on_log_line(line: str) -> None:
        log_lines.append(line)
        if "Received UDP:" in line:
            receive_event.set()

    async with udp_listener() as (broadcast_port, receiver):
        listen_port = _get_free_udp_port()
        config = yaml_config.replace("UDP_LISTEN_PORT_PLACEHOLDER", str(listen_port))
        config = config.replace("UDP_BROADCAST_PORT_PLACEHOLDER", str(broadcast_port))

        async with (
            run_compiled(config, line_callback=on_log_line),
            api_client_connected() as client,
        ):
            # Verify device is running
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "udp-test"

            # Get services
            _, services = await client.list_entities_services()

            # Test sending string message
            send_message_service = next(
                (s for s in services if s.name == "send_udp_message"), None
            )
            assert send_message_service is not None, (
                "send_udp_message service not found"
            )

            await client.execute_service(send_message_service, {})

            try:
                msg = await receiver.wait_for_content(b"HELLO_UDP_TEST", timeout=5.0)
                assert b"HELLO_UDP_TEST" in msg
            except TimeoutError:
                pytest.fail(
                    f"UDP string message not received. Got: {receiver.messages}"
                )

            # Test sending bytes
            send_bytes_service = next(
                (s for s in services if s.name == "send_udp_bytes"), None
            )
            assert send_bytes_service is not None, "send_udp_bytes service not found"

            await client.execute_service(send_bytes_service, {})

            try:
                msg = await receiver.wait_for_content(b"UDP_BYTES", timeout=5.0)
                assert b"UDP_BYTES" in msg
            except TimeoutError:
                pytest.fail(f"UDP bytes message not received. Got: {receiver.messages}")

            # Verify we received at least 2 messages (string + bytes)
            assert len(receiver.messages) >= 2, (
                f"Expected at least 2 messages, got {len(receiver.messages)}"
            )

            # Verify dump_config logged all configured addresses
            # This tests that FixedVector<const char*> stores addresses correctly
            log_text = "\n".join(log_lines)
            assert "Address: 127.0.0.1" in log_text, (
                f"Address 127.0.0.1 not found in dump_config. Log: {log_text[-2000:]}"
            )
            assert "Address: 127.0.0.2" in log_text, (
                f"Address 127.0.0.2 not found in dump_config. Log: {log_text[-2000:]}"
            )

            # Test receiving a UDP packet (exercises on_receive with std::span)
            test_payload = b"TEST_RECEIVE_UDP"
            send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            try:
                send_sock.sendto(test_payload, ("127.0.0.1", listen_port))
            finally:
                send_sock.close()

            try:
                await asyncio.wait_for(receive_event.wait(), timeout=5.0)
            except TimeoutError:
                pytest.fail(
                    f"on_receive did not fire. Expected 'Received UDP:' in logs. "
                    f"Last log lines: {log_lines[-20:]}"
                )
