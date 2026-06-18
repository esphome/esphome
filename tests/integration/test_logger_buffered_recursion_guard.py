"""Integration test for the recursion guard on the buffered logger drain path.

Regression test for a crash where a log message drained from the task log buffer
(i.e. logged from a non-main thread) re-entered the logger on the main task while it
was still being delivered to listeners. The buffered drain in
``Logger::process_messages_`` did not hold the main-task recursion guard that the
synchronous logging path holds, so a listener callback that logged again on the main
task (e.g. the API log-forwarding path, or a ``logger.on_message`` automation) reused
the shared ``tx_buffer_`` and clobbered the message mid-delivery. On ESP32 this showed
up as a ``StoreProhibited`` panic inside the API send path.

The fixture logs a small batch of verifiable messages from a non-main thread (kept
under the host task-log-buffer slot count so they all take the buffered drain path
rather than the emergency console fallback) while an ``on_message`` automation re-logs
``REENTRANT_CLOBBER_MARKER`` on the main task for every delivered message.

Without the guard the re-entrant marker is written into the shared ``tx_buffer_`` while
the buffered thread message is still being delivered, so the message the API receives is
contaminated (it contains the marker and an embedded newline glued onto the thread
payload). With the guard the re-entrant log is dropped during the drain, the marker
never appears, and every thread message is delivered clean.
"""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import LogLevel
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

_ANSI = re.compile(r"\x1b\[[0-9;]*m")
# THREADMSGnnn_DATA_xxxxxxxx where data is a deterministic checksum of the index
THREAD_MSG_PATTERN = re.compile(r"THREADMSG(\d{3})_DATA_([0-9A-F]{8})")

NUM_MESSAGES = 30


@pytest.mark.asyncio
async def test_logger_buffered_recursion_guard(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Buffered (non-main-thread) log messages survive a re-entrant main-task log."""
    console_lines: list[str] = []
    api_messages: list[str] = []
    test_complete_event = asyncio.Event()

    def line_callback(line: str) -> None:
        console_lines.append(line)
        if "RACE_TEST_COMPLETE" in line:
            test_complete_event.set()

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "logger-recursion-test"

        # Subscribe over the API: this is the exact path that crashed in the field
        # (the API log callback runs during the buffered drain). The API message field
        # preserves embedded newlines, so it reliably exposes a clobbered buffer.
        def on_log(msg) -> None:
            api_messages.append(msg.message.decode("utf-8", errors="replace"))

        client.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_VERY_VERBOSE)

        entities, _ = await client.list_entities_services()
        buttons = [e for e in entities if e.name == "Start Race Test"]
        assert buttons, "Could not find Start Race Test button"
        client.button_command(buttons[0].key)

        # RACE_TEST_COMPLETE is logged from the main task, so it arrives even if the
        # buffered path is corrupted. The buffered messages are drained afterwards.
        try:
            await asyncio.wait_for(test_complete_event.wait(), timeout=30.0)
        except TimeoutError:
            pytest.fail(
                "Test did not complete within timeout; device likely crashed or hung. "
                f"Collected {len(console_lines)} console lines."
            )

        # Allow the buffered messages to drain after the thread joined.
        await asyncio.sleep(0.5)

    intact: set[int] = set()
    contaminated: list[str] = []
    for raw in api_messages:
        text = _ANSI.sub("", raw)
        if "THREADMSG" not in text:
            continue
        # A clean thread message is a single line carrying only its own payload. A
        # clobbered buffer glues the re-entrant marker (and an embedded newline) onto it.
        if "REENTRANT" in text or "\n" in text:
            contaminated.append(repr(raw))
            continue
        match = THREAD_MSG_PATTERN.search(text)
        assert match, f"Unexpected thread message format: {raw!r}"
        msg_num = int(match.group(1))
        expected = f"{msg_num * 12345:08X}"
        if match.group(2) != expected:
            contaminated.append(repr(raw))
            continue
        intact.add(msg_num)

    assert not contaminated, (
        "Buffered thread messages were clobbered by a re-entrant main-task log "
        "(missing recursion guard on the buffered drain path):\n"
        + "\n".join(contaminated[:10])
    )
    assert len(intact) == NUM_MESSAGES, (
        f"Expected {NUM_MESSAGES} intact buffered thread messages over the API, got "
        f"{len(intact)}. Missing ids: {sorted(set(range(NUM_MESSAGES)) - intact)}"
    )
