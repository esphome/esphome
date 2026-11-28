"""Test that debug_scheduler aborts when a capture's destructor calls the scheduler.

The scheduler frees a finished callback's capture with its lock held, so a
destructor that schedules or cancels a timer deadlocks on ESP32, LibreTiny and
host, and on ESP8266 and RP2 can cancel itself or be lost. A debug_scheduler
build aborts there instead, and still allows it on the SCHEDULER_DONT_RUN path,
which frees the capture after releasing the lock.
"""

from __future__ import annotations

import asyncio
import signal

import pytest

from .conftest import run_binary
from .log_utils import LineWaiter
from .types import CompileFunction, ConfigWriter


@pytest.mark.asyncio
async def test_scheduler_capture_destructor_guard(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
) -> None:
    """The DONT_RUN free reschedules; the free after a timeout runs aborts."""
    waiter = LineWaiter()
    binary_path = await compile_esphome(await write_yaml_config(yaml_config))

    async with run_binary(binary_path, line_callback=waiter.callback) as (proc, _):
        await waiter.wait_for_each(
            "DONT_RUN_FREED",
            "RESCHEDULED_RAN",
            "TIMEOUT_RAN",
            "capture called the scheduler from its destructor",
            timeout=5.0,
        )
        await asyncio.wait_for(proc.wait(), timeout=5.0)
        assert proc.returncode == -signal.SIGABRT
