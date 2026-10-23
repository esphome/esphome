"""Integration test for CachedGPIOExpander to ensure correct behavior."""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

ANSI_PATTERN = re.compile(r"\x1b\[[0-9;]*m")
# Any variation of digital_read, anywhere in the message
READ_PATTERN = re.compile(r"(?:uint16_)?digital_read_(?:hw|cache) pin=\d+")
# The whole read message, so a prefixed read is reported as a mismatch
READ_MESSAGE_PATTERN = re.compile(r"^(\w+) pin=(\d+)")

HW = "digital_read_hw"
CACHE = "digital_read_cache"
UINT16_HW = "uint16_digital_read_hw"
UINT16_CACHE = "uint16_digital_read_cache"

EXPECTED_READS: list[tuple[str, int]] = [
    (HW, 0),
    *[(CACHE, i) for i in range(8)],
    (HW, 8),
    *[(CACHE, i) for i in range(8, 16)],
    (HW, 16),
    *[(CACHE, i) for i in range(16, 24)],
    (HW, 24),
    *[(CACHE, i) for i in range(24, 32)],
    (HW, 3),
    (CACHE, 3),
    (HW, 3),
    (CACHE, 3),
    (CACHE, 4),
    (HW, 3),
    (CACHE, 3),
    (HW, 10),
    (CACHE, 10),
    # full cache reset here for testing
    (HW, 15),
    (CACHE, 15),
    (CACHE, 14),
    (HW, 14),
    (CACHE, 14),
    # uint16_t component tests (single bank of 16 pins)
    (UINT16_HW, 0),  # First pin triggers hw read
    *[(UINT16_CACHE, i) for i in range(16)],  # All 16 pins return via cache
    # After cache reset
    (UINT16_HW, 5),  # First read after reset triggers hw
    (UINT16_CACHE, 5),
    (UINT16_CACHE, 10),  # These use cache (same bank)
    (UINT16_CACHE, 15),
    (UINT16_CACHE, 0),
]


def _read(line: str) -> tuple[str, int] | str | None:
    """Return (name, pin) for a read log line, the message if it is malformed, else None."""
    clean_line = ANSI_PATTERN.sub("", line)
    msg = clean_line.split(": ", 1)[-1]
    if not READ_PATTERN.search(msg):
        return None
    if match := READ_MESSAGE_PATTERN.search(msg):
        return match.group(1), int(match.group(2))
    return msg


@pytest.mark.asyncio
async def test_gpio_expander_cache(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test gpio_expander::CachedGpioExpander correctly calls hardware functions."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "gpio-expander-cache")

        done_line = await wait_or_fail(
            waiter.wait_for("DONE_UINT16"),
            5.0,
            "Timeout waiting for logs to complete",
        )

    # Only the reads logged before the uint16 component finished count
    before_done = waiter.lines[: waiter.lines.index(done_line)]
    reads = [read for line in before_done if (read := _read(line)) is not None]
    assert reads == EXPECTED_READS
