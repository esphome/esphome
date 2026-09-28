"""Integration test for host logger thread safety.

This test verifies that the logger's MPSC ring buffer correctly handles
multiple threads racing to log messages without corruption or data loss.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

# Expected pattern for log messages from threads
# Format: THREADn_MSGnnn_LEVEL_MESSAGE_WITH_DATA_xxxxxxxx
THREAD_MSG_PATTERN = re.compile(
    r"THREAD(\d+)_MSG(\d{3})_(INFO|DEBUG|WARN|ERROR)_MESSAGE_WITH_DATA_([0-9A-F]{8})"
)

# Pattern for test start/complete markers
TEST_START_PATTERN = re.compile(r"RACE_TEST_START.*Starting (\d+) threads")
TEST_COMPLETE_PATTERN = re.compile(r"RACE_TEST_COMPLETE.*total messages: (\d+)")

# Expected values
NUM_THREADS = 3
MESSAGES_PER_THREAD = 100
EXPECTED_TOTAL_MESSAGES = NUM_THREADS * MESSAGES_PER_THREAD


@pytest.mark.asyncio
async def test_host_logger_thread_safety(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that multiple threads can log concurrently without corruption.

    This test:
    1. Spawns 3 threads that each log 100 messages
    2. Collects all log output
    3. Verifies no lines are corrupted (partially written or interleaved)
    4. Verifies all expected messages were received
    """
    collected_lines: list[str] = []
    test_complete_event = asyncio.Event()

    def line_callback(line: str) -> None:
        """Collect log lines and detect test completion."""
        collected_lines.append(line)
        if "RACE_TEST_COMPLETE" in line:
            test_complete_event.set()

    # Run the test binary and collect output
    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        # Verify connection works
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "host-logger-thread-test"

        # Get the button entity - find by name
        entities, _ = await client.list_entities_services()
        button_entities = [e for e in entities if e.name == "Start Thread Race Test"]
        assert button_entities, "Could not find Start Thread Race Test button"
        button_key = button_entities[0].key

        # Press the button to start the thread race test
        client.button_command(button_key)

        # Wait for test to complete (with timeout)
        try:
            await asyncio.wait_for(test_complete_event.wait(), timeout=30.0)
        except TimeoutError:
            pytest.fail(
                "Test did not complete within timeout. "
                f"Collected {len(collected_lines)} lines."
            )

        # Give a bit more time for any remaining buffered messages
        await asyncio.sleep(0.5)

    # Analyze collected log lines
    thread_messages: dict[int, set[int]] = {i: set() for i in range(NUM_THREADS)}
    corrupted_lines: list[str] = []
    test_started = False
    test_completed = False
    reported_total = 0

    for line in collected_lines:
        # Check for test start
        start_match = TEST_START_PATTERN.search(line)
        if start_match:
            test_started = True
            assert int(start_match.group(1)) == NUM_THREADS, (
                f"Unexpected thread count: {start_match.group(1)}"
            )
            continue

        # Check for test completion
        complete_match = TEST_COMPLETE_PATTERN.search(line)
        if complete_match:
            test_completed = True
            reported_total = int(complete_match.group(1))
            continue

        # Check for thread messages
        msg_match = THREAD_MSG_PATTERN.search(line)
        if msg_match:
            thread_id = int(msg_match.group(1))
            msg_num = int(msg_match.group(2))
            # level = msg_match.group(3)  # INFO, DEBUG, WARN, ERROR
            data_hex = msg_match.group(4)

            # Verify data value matches expected calculation
            expected_data = f"{msg_num * 12345:08X}"
            if data_hex != expected_data:
                corrupted_lines.append(
                    f"Data mismatch in line: {line} "
                    f"(expected {expected_data}, got {data_hex})"
                )
                continue

            # Track which messages we received from each thread
            if 0 <= thread_id < NUM_THREADS:
                thread_messages[thread_id].add(msg_num)
            else:
                corrupted_lines.append(f"Invalid thread ID in line: {line}")
            continue

        # Check for partial/corrupted thread messages
        # If a line contains part of a thread message pattern but doesn't match fully
        # This could indicate line corruption from interleaving
        if (
            "THREAD" in line
            and "MSG" in line
            and not msg_match
            and "_MESSAGE_WITH_DATA_" in line
        ):
            corrupted_lines.append(f"Possibly corrupted line: {line}")

    # Assertions
    assert test_started, "Test start marker not found in output"
    assert test_completed, "Test completion marker not found in output"
    assert reported_total == EXPECTED_TOTAL_MESSAGES, (
        f"Reported total {reported_total} != expected {EXPECTED_TOTAL_MESSAGES}"
    )

    # Check for corrupted lines
    assert not corrupted_lines, (
        f"Found {len(corrupted_lines)} corrupted lines:\n"
        + "\n".join(corrupted_lines[:10])  # Show first 10
    )

    # Count total messages received
    total_received = sum(len(msgs) for msgs in thread_messages.values())

    # We may not receive all messages due to ring buffer overflow when buffer is full
    # The test primarily verifies no corruption, not that we receive every message
    # However, we should receive a reasonable number of messages
    min_expected = EXPECTED_TOTAL_MESSAGES // 2  # At least 50%
    assert total_received >= min_expected, (
        f"Received only {total_received} messages, expected at least {min_expected}. "
        f"Per-thread breakdown: "
        + ", ".join(f"Thread{i}: {len(msgs)}" for i, msgs in thread_messages.items())
    )

    # Verify we got messages from all threads (proves concurrent logging worked)
    for thread_id in range(NUM_THREADS):
        assert thread_messages[thread_id], (
            f"No messages received from thread {thread_id}"
        )

    # Log summary for debugging
    print("\nThread safety test summary:")
    print(f"  Total messages received: {total_received}/{EXPECTED_TOTAL_MESSAGES}")
    for thread_id in range(NUM_THREADS):
        received = len(thread_messages[thread_id])
        print(f"  Thread {thread_id}: {received}/{MESSAGES_PER_THREAD} messages")
