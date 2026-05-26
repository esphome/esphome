"""Integration test for syslog component."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncGenerator
import contextlib
from contextlib import asynccontextmanager
from dataclasses import dataclass, field
import re
import socket
from typing import TypedDict

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


class ParsedSyslogMessage(TypedDict):
    """Parsed syslog message components."""

    pri: int
    facility: int
    severity: int
    timestamp: str
    hostname: str
    tag: str
    message: str


# RFC 3164 syslog message pattern:
# <PRI>TIMESTAMP HOSTNAME TAG: MESSAGE
# Example: <134>Dec 20 14:30:45 syslog-test app: [D][app:029]: Running...
SYSLOG_PATTERN = re.compile(
    r"<(\d+)>"  # PRI (priority = facility * 8 + severity)
    r"(\S+ +\d+ \d+:\d+:\d+|-)"  # TIMESTAMP (BSD-style "%b %e %H:%M:%S", e.g. "Dec 20 14:30:45", or NILVALUE "-")
    r" (\S+)"  # HOSTNAME
    r" (\S+):"  # TAG
    r" (.*)"  # MESSAGE
)


@dataclass
class SyslogReceiver:
    """Collects syslog messages received over UDP."""

    messages: list[str] = field(default_factory=list)
    message_received: asyncio.Event = field(default_factory=asyncio.Event)
    _waiters: list[tuple[re.Pattern, asyncio.Event]] = field(default_factory=list)

    def on_message(self, msg: str) -> None:
        """Called when a message is received."""
        self.messages.append(msg)
        self.message_received.set()
        # Check pattern waiters
        for pattern, event in self._waiters:
            if pattern.search(msg):
                event.set()

    async def wait_for_messages(self, timeout: float = 10.0) -> None:
        """Wait for at least one message to be received."""
        await asyncio.wait_for(self.message_received.wait(), timeout=timeout)

    async def wait_for_pattern(self, pattern: str, timeout: float = 5.0) -> str:
        """Wait for a message matching the pattern."""
        compiled = re.compile(pattern)
        event = asyncio.Event()
        self._waiters.append((compiled, event))
        try:
            # Check existing messages first
            for msg in self.messages:
                if compiled.search(msg):
                    return msg
            # Wait for new message
            await asyncio.wait_for(event.wait(), timeout=timeout)
            # Find and return the matching message
            for msg in reversed(self.messages):
                if compiled.search(msg):
                    return msg
            raise RuntimeError("Event set but no matching message found")
        finally:
            self._waiters.remove((compiled, event))


@asynccontextmanager
async def syslog_udp_listener() -> AsyncGenerator[tuple[int, SyslogReceiver]]:
    """Async context manager that listens for syslog UDP messages.

    Yields:
        Tuple of (port, SyslogReceiver) where port is the UDP port to send to
        and SyslogReceiver contains the received messages.
    """
    # Create and bind UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("127.0.0.1", 0))
    sock.setblocking(False)
    port = sock.getsockname()[1]

    receiver = SyslogReceiver()

    async def receive_messages() -> None:
        """Background task to receive syslog messages."""
        loop = asyncio.get_running_loop()
        while True:
            try:
                data = await loop.sock_recv(sock, 4096)
                if data:
                    msg = data.decode("utf-8", errors="replace")
                    receiver.on_message(msg)
            except BlockingIOError:
                await asyncio.sleep(0.01)
            except Exception:
                break

    task = asyncio.create_task(receive_messages())
    try:
        yield port, receiver
    finally:
        task.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await task
        sock.close()


def parse_syslog_message(msg: str) -> ParsedSyslogMessage | None:
    """Parse a syslog message and return its components."""
    match = SYSLOG_PATTERN.match(msg)
    if not match:
        return None
    pri, timestamp, hostname, tag, message = match.groups()
    pri_val = int(pri)
    # PRI = facility * 8 + severity
    facility = pri_val // 8
    severity = pri_val % 8
    return ParsedSyslogMessage(
        pri=pri_val,
        facility=facility,
        severity=severity,
        timestamp=timestamp,
        hostname=hostname,
        tag=tag,
        message=message,
    )


@pytest.mark.asyncio
async def test_syslog(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test syslog component sends properly formatted messages."""
    async with syslog_udp_listener() as (udp_port, receiver):
        # Replace the placeholder port in the config
        config = yaml_config.replace("SYSLOG_PORT_PLACEHOLDER", str(udp_port))

        async with run_compiled(config), api_client_connected() as client:
            # Verify device is running
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "syslog-test"

            # Wait for syslog messages (ESPHome logs during startup)
            try:
                await receiver.wait_for_messages(timeout=10.0)
            except TimeoutError:
                pytest.fail("No syslog messages received within timeout")

            # Give it a moment to collect more messages
            await asyncio.sleep(0.5)

            # Verify we received messages
            assert len(receiver.messages) > 0, "No syslog messages received"

            # Parse and validate all messages
            parsed_messages: list[ParsedSyslogMessage] = []
            for msg in receiver.messages:
                parsed = parse_syslog_message(msg)
                if parsed:
                    parsed_messages.append(parsed)

            assert len(parsed_messages) > 0, (
                f"No valid syslog messages found. Received: {receiver.messages[:5]}"
            )

            # Validate message format for all parsed messages
            for parsed in parsed_messages:
                # Validate PRI is in valid range (0-191)
                assert 0 <= parsed["pri"] <= 191, f"Invalid PRI: {parsed['pri']}"

                # Validate facility matches config (16 = local0)
                assert parsed["facility"] == 16, (
                    f"Expected facility 16, got {parsed['facility']}"
                )

                # Validate severity is in valid range (0-7)
                assert 0 <= parsed["severity"] <= 7, (
                    f"Invalid severity: {parsed['severity']}"
                )

                # Validate hostname matches device name
                assert parsed["hostname"] == "syslog-test", (
                    f"Unexpected hostname: {parsed['hostname']}"
                )

                # Validate timestamp format (BSD or NILVALUE)
                if parsed["timestamp"] != "-":
                    assert re.match(
                        r"[A-Z][a-z]{2} +\d+ \d{2}:\d{2}:\d{2}",
                        parsed["timestamp"],
                    ), f"Invalid timestamp format: {parsed['timestamp']}"

            # Verify we see different severity levels in the logs
            severities_seen = {p["severity"] for p in parsed_messages}
            # ESPHome startup logs should include at least INFO (5) or DEBUG (7)
            assert len(severities_seen) >= 1, "Expected to see at least one severity"

            # Verify messages don't contain ANSI color codes (strip=true)
            for parsed in parsed_messages:
                assert "\x1b[" not in parsed["message"], (
                    f"Color codes not stripped: {parsed['message'][:50]}"
                )

            # Verify message content is not empty for most messages
            non_empty_messages = [p for p in parsed_messages if p["message"].strip()]
            assert len(non_empty_messages) > 0, "All messages are empty"

            # Verify tag format (should be component name like "app", "wifi", etc.)
            for parsed in parsed_messages:
                assert len(parsed["tag"]) > 0, "Empty tag"
                # Tag should not contain spaces or colons
                assert " " not in parsed["tag"], f"Tag contains space: {parsed['tag']}"

            # Test message truncation - call service that logs a very long message
            _, services = await client.list_entities_services()
            log_service = next(
                (s for s in services if s.name == "log_long_message"), None
            )
            assert log_service is not None, "log_long_message service not found"

            # Call the service to trigger a long log message
            await client.execute_service(log_service, {})

            # Wait specifically for the truncation test message
            try:
                trunc_msg = await receiver.wait_for_pattern(r"trunctest.*START\|")
            except TimeoutError:
                pytest.fail(
                    f"Truncation test message not received. Got: {receiver.messages}"
                )

            # Verify message is truncated to max 508 bytes
            assert len(trunc_msg) <= 508, f"Message exceeds 508 bytes: {len(trunc_msg)}"

            # Verify the message starts correctly but is truncated (no "|END")
            assert "START|" in trunc_msg, "Message should contain START marker"
            assert "|END" not in trunc_msg, (
                "Message should be truncated before END marker"
            )

            # Test short message - should arrive complete (not truncated)
            short_service = next(
                (s for s in services if s.name == "log_short_message"), None
            )
            assert short_service is not None, "log_short_message service not found"

            await client.execute_service(short_service, {})

            try:
                short_msg = await receiver.wait_for_pattern(r"shorttest.*BEGIN\|")
            except TimeoutError:
                pytest.fail(
                    f"Short test message not received. Got: {receiver.messages[-10:]}"
                )

            # Verify short message arrived complete with both markers
            assert "BEGIN|" in short_msg, "Short message missing BEGIN marker"
            assert "|FINISH" in short_msg, (
                f"Short message truncated unexpectedly: {short_msg}"
            )
            assert "SHORT_MESSAGE_CONTENT" in short_msg, (
                f"Short message content missing: {short_msg}"
            )
